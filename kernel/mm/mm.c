#include <kernel/mm.h>
#include <kernel/string.h>

#define PAGE_SIZE 4096ULL
#define PTE_PRESENT (1ULL << 0)
#define PTE_WRITE   (1ULL << 1)
#define PTE_USER    (1ULL << 2)
#define ADDR_MASK   0x000ffffffffff000ULL

static struct limine_memmap_response *memmap;
static uint64_t hhdm;
static uint64_t region_index;
static uint64_t next_phys;
static uint64_t region_end;

static uint64_t align_up(uint64_t x, uint64_t a) {
    return (x + a - 1) & ~(a - 1);
}

void mm_init(struct limine_memmap_response *map, uint64_t hhdm_offset) {
    memmap = map;
    hhdm = hhdm_offset;
    region_index = 0;
    next_phys = 0;
    region_end = 0;
}

void *phys_to_virt(uint64_t phys) {
    return (void *)(uintptr_t)(phys + hhdm);
}

uint64_t pmm_alloc_page(void) {
    for (;;) {
        if (next_phys && next_phys + PAGE_SIZE <= region_end) {
            uint64_t page = next_phys;
            next_phys += PAGE_SIZE;
            memset(phys_to_virt(page), 0, PAGE_SIZE);
            return page;
        }

        while (region_index < memmap->entry_count) {
            struct limine_memmap_entry *e = memmap->entries[region_index++];
            if (e->type != LIMINE_MEMMAP_USABLE || e->length < PAGE_SIZE) continue;
            next_phys = align_up(e->base, PAGE_SIZE);
            region_end = e->base + e->length;
            break;
        }

        if (!next_phys || next_phys + PAGE_SIZE > region_end) return 0;
    }
}

static uint64_t read_cr3(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(value));
    return value & ADDR_MASK;
}

static uint64_t *next_table(uint64_t *table, size_t index) {
    uint64_t entry = table[index];
    if (!(entry & PTE_PRESENT)) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) return 0;
        table[index] = phys | PTE_PRESENT | PTE_WRITE | PTE_USER;
        return phys_to_virt(phys);
    }

    table[index] |= PTE_USER;
    return phys_to_virt(entry & ADDR_MASK);
}

void vmm_map_user(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t *pml4 = phys_to_virt(read_cr3());
    size_t i4 = (virt >> 39) & 0x1ff;
    size_t i3 = (virt >> 30) & 0x1ff;
    size_t i2 = (virt >> 21) & 0x1ff;
    size_t i1 = (virt >> 12) & 0x1ff;

    uint64_t *pdpt = next_table(pml4, i4);
    uint64_t *pd   = next_table(pdpt, i3);
    uint64_t *pt   = next_table(pd, i2);
    if (!pdpt || !pd || !pt) return;

    uint64_t bits = PTE_PRESENT | PTE_USER;
    if (flags & VMM_WRITE) bits |= PTE_WRITE;
    pt[i1] = (phys & ADDR_MASK) | bits;
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}
