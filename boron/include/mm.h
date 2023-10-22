// Boron - Memory Manager
#ifndef NS64_MM_H
#define NS64_MM_H

#include <main.h>
#include <arch.h>

// Debug flags. Use if something's gone awry
#define MM_DBG_NO_DEMAND_PAGING (0)

typedef struct
{
	uint64_t entries[512];
}
PageMapLevel;

#define PTE_ADDRESS(pte) ((pte) & MM_PTE_ADDRESSMASK)

#include <mm/pfn.h>
#include <mm/pmm.h>
#include <mm/pt.h>

// Initialize the allocators.
void MmInitAllocators();

#endif//NS64_MM_H