#include <kernel/mm.h>
#include <kernel/string.h>

#define PTE_PRESENT (1ULL << 0)
#define PTE_WRITE   (1ULL << 1)
#define PTE_USER    (1ULL << 2)
#define PTE_HUGE    (1ULL << 7)
#define ADDR_MASK   0x000ffffffffff000ULL

static struct limine_memmap_response *memmap;
static uint64_t hhdm;
static uint64_t region_index;
static uint64_t next_phys;
static uint64_t region_end;
static uint64_t free_head;

static uint64_t align_up(uint64_t x, uint64_t a) {
    return (x + a - 1) & ~(a - 1);
}

void mm_init(struct limine_memmap_response *map, uint64_t hhdm_offset) {
    memmap = map;
    hhdm = hhdm_offset;
    region_index = 0;
    next_phys = 0;
    region_end = 0;
    free_head = 0;
}

void *phys_to_virt(uint64_t phys) {
    return (void *)(uintptr_t)(phys + hhdm);
}

uint64_t pmm_alloc_page(void) {
    if (free_head) {
        uint64_t page = free_head;
        free_head = *(uint64_t *)phys_to_virt(page);
        memset(phys_to_virt(page), 0, PAGE_SIZE);
        return page;
    }

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

void pmm_free_page(uint64_t phys) {
    if (!phys || (phys & (PAGE_SIZE - 1))) return;
    *(uint64_t *)phys_to_virt(phys) = free_head;
    free_head = phys;
}

static uint64_t read_cr3(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(value));
    return value & ADDR_MASK;
}

static uint64_t *next_table(uint64_t *table, size_t index, int create, int user) {
    uint64_t entry = table[index];
    if (!(entry & PTE_PRESENT)) {
        if (!create) return 0;
        uint64_t phys = pmm_alloc_page();
        if (!phys) return 0;
        uint64_t bits = PTE_PRESENT | PTE_WRITE;
        if (user) bits |= PTE_USER;
        table[index] = phys | bits;
        return phys_to_virt(phys);
    }

    if (entry & PTE_HUGE) return 0;
    if (user && create) table[index] |= PTE_USER;
    return phys_to_virt(entry & ADDR_MASK);
}

static uint64_t *get_pte(uint64_t virt, int create, int user) {
    uint64_t *pml4 = phys_to_virt(read_cr3());
    size_t i4 = (virt >> 39) & 0x1ff;
    size_t i3 = (virt >> 30) & 0x1ff;
    size_t i2 = (virt >> 21) & 0x1ff;
    size_t i1 = (virt >> 12) & 0x1ff;

    uint64_t *pdpt = next_table(pml4, i4, create, user);
    if (!pdpt) return 0;
    uint64_t *pd = next_table(pdpt, i3, create, user);
    if (!pd) return 0;
    uint64_t *pt = next_table(pd, i2, create, user);
    if (!pt) return 0;
    return &pt[i1];
}

static void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags, int user) {
    uint64_t *pte = get_pte(virt, 1, user);
    if (!pte) return;

    uint64_t bits = PTE_PRESENT;
    if (user) bits |= PTE_USER;
    if (flags & VMM_WRITE) bits |= PTE_WRITE;
    *pte = (phys & ADDR_MASK) | bits;
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_map_user(uint64_t virt, uint64_t phys, uint64_t flags) {
    vmm_map(virt, phys, flags, 1);
}


int vmm_protect_user(uint64_t virt, uint64_t flags) {
    uint64_t *pte = get_pte(virt, 0, 0);
    if (!pte || !(*pte & PTE_PRESENT) || !(*pte & PTE_USER)) return -1;
    if (flags & VMM_WRITE) *pte |= PTE_WRITE;
    else *pte &= ~PTE_WRITE;
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
    return 0;
}

int vmm_unmap_user(uint64_t virt) {
    uint64_t *pte = get_pte(virt, 0, 0);
    if (!pte || !(*pte & PTE_PRESENT) || !(*pte & PTE_USER)) return -1;
    uint64_t phys = *pte & ADDR_MASK;
    *pte = 0;
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
    pmm_free_page(phys);
    return 0;
}

uint64_t vmm_user_phys(uint64_t virt) {
    uint64_t *pte = get_pte(virt, 0, 0);
    if (!pte || !(*pte & PTE_PRESENT) || !(*pte & PTE_USER)) return 0;
    return (*pte & ADDR_MASK) | (virt & (PAGE_SIZE - 1));
}

int vmm_user_range_ok(uint64_t virt, uint64_t len, int write) {
    if (!len) return 1;
    if (virt > 0x00007fffffffffffULL) return 0;
    if (len - 1 > 0x00007fffffffffffULL - virt) return 0;

    uint64_t end = virt + len - 1;
    uint64_t page = virt & ~(PAGE_SIZE - 1);
    uint64_t last = end & ~(PAGE_SIZE - 1);
    for (;;) {
        uint64_t *pte = get_pte(page, 0, 0);
        if (!pte || !(*pte & PTE_PRESENT) || !(*pte & PTE_USER)) return 0;
        if (write && !(*pte & PTE_WRITE)) return 0;
        if (page == last) break;
        page += PAGE_SIZE;
    }
    return 1;
}
