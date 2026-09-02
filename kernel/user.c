#include <kernel/mm.h>
#include <kernel/string.h>
#include <kernel/vga.h>
#include <stdint.h>

#define USER_BASE       0x0000100000000000ULL
#define USER_STACK_TOP  0x0000100000200000ULL
#define USER_STACK_PAGES 4
#define PAGE_SIZE 4096ULL

extern const unsigned char user_blob_start[];
extern const unsigned char user_blob_end[];
extern void enter_userspace(uint64_t entry, uint64_t stack);

void user_start(void) {
    uint64_t size = (uint64_t)(user_blob_end - user_blob_start);
    uint64_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint64_t i = 0; i < pages; i++) {
        uint64_t phys = pmm_alloc_page();
        uint64_t offset = i * PAGE_SIZE;
        uint64_t copy = size - offset;
        if (copy > PAGE_SIZE) copy = PAGE_SIZE;
        memcpy(phys_to_virt(phys), user_blob_start + offset, copy);
        vmm_map_user(USER_BASE + offset, phys, 0);
    }

    for (uint64_t i = 0; i < USER_STACK_PAGES; i++) {
        uint64_t phys = pmm_alloc_page();
        uint64_t va = USER_STACK_TOP - (USER_STACK_PAGES - i) * PAGE_SIZE;
        vmm_map_user(va, phys, VMM_WRITE);
    }

    vga_puts("[user] entering ring 3\n");
    enter_userspace(USER_BASE, USER_STACK_TOP - 16);
}
