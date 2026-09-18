#pragma once
#include <stddef.h>
#include <stdint.h>
#include "limine.h"

#define PAGE_SIZE 4096ULL
#define VMM_WRITE (1ULL << 1)

void mm_init(struct limine_memmap_response *map, uint64_t hhdm_offset);
uint64_t pmm_alloc_page(void);
void pmm_free_page(uint64_t phys);
void *phys_to_virt(uint64_t phys);
void vmm_map_user(uint64_t virt, uint64_t phys, uint64_t flags);
int vmm_protect_user(uint64_t virt, uint64_t flags);
int vmm_unmap_user(uint64_t virt);
int vmm_user_range_ok(uint64_t virt, uint64_t len, int write);
uint64_t vmm_user_phys(uint64_t virt);
