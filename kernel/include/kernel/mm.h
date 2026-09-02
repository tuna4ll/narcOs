#pragma once
#include <stddef.h>
#include <stdint.h>
#include "limine.h"

void mm_init(struct limine_memmap_response *map, uint64_t hhdm_offset);
uint64_t pmm_alloc_page(void);
void *phys_to_virt(uint64_t phys);
void vmm_map_user(uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_map_kernel(uint64_t virt, uint64_t phys, uint64_t flags);

#define VMM_WRITE (1ULL << 1)
