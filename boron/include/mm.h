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
#include <mm/pool.h>
#include <mm/pt.h>
#include <mm/probe.h>
#include <mm/mdl.h>
#include <mm/cache.h>

#ifdef KERNEL
// Initialize the allocators.
void MmInitAllocators();
#endif

#endif//NS64_MM_H