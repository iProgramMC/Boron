/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/i386/idmap.c
	
Abstract:
	This module implements identity mapping management for
	the i386 platform.
	
Author:
	iProgramInCpp - 15 October 2025
***/
#include "../mi.h"

#define P2V(n) ((void*)(MI_IDENTMAP_START + (n))
#define V2P(p) ((uintptr_t)(p) - MI_IDENTMAP_START)

extern uint8_t KiBootstrapPageTables[];
extern uint8_t KiHhdmWindowPageTables[];
extern uint8_t KiPoolHeadersPageTables[];

void MiInitializeBaseIdentityMapping()
{
	for (size_t i = 0; i < MI_IDENTMAP_SIZE; i += PAGE_SIZE)
	{
		uintptr_t Address = MI_IDENTMAP_START + i;
		
		PMMPTE Level2 = (PMMPTE) MI_PTE_LOC(MI_PTE_LOC(Address));
		PMMPTE Level1 = (PMMPTE) MI_PTE_LOC(Address);
		
		*Level2 = V2P(&KiBootstrapPageTables[(i >> 22) * PAGE_SIZE])
			| MM_PTE_READWRITE
			| MM_PTE_PRESENT;
		
		*Level1 = i | MM_PTE_READWRITE | MM_PTE_PRESENT;
	}
	
	// Map the level 2 PTs for the HHDM window.
	for (size_t i = 0; i < MI_FASTMAP_SIZE; i += PAGE_SIZE * PAGE_SIZE / sizeof(MMPTE))
	{
		uintptr_t Address = MI_FASTMAP_START + i;
		
		PMMPTE Level2 = (PMMPTE) MI_PTE_LOC(MI_PTE_LOC(Address));
		*Level2 = V2P(&KiHhdmWindowPageTables[(i >> 22) * PAGE_SIZE])
			| MM_PTE_READWRITE
			| MM_PTE_PRESENT;
	}
	
	// Map the level 2 PTs for the pool headers.
	for (size_t i = 0; i < MI_POOL_HEADERS_SIZE; i += PAGE_SIZE * PAGE_SIZE / sizeof(MMPTE))
	{
		uintptr_t Address = MI_POOL_HEADERS_START + i;
		
		PMMPTE Level2 = (PMMPTE) MI_PTE_LOC(MI_PTE_LOC(Address));
		*Level2 = V2P(&KiPoolHeadersPageTables[(i >> 22) * PAGE_SIZE])
			| MM_PTE_READWRITE
			| MM_PTE_PRESENT;
	}
}
