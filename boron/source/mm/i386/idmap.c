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
	PMMPTE Level2, Level1;
	uintptr_t Address;
	
	for (size_t i = 0; i < MI_IDENTMAP_SIZE; i += PAGE_SIZE)
	{
		Address = MI_IDENTMAP_START + i;
		
		Level2 = (PMMPTE) MI_PTE_LOC(MI_PTE_LOC(Address));
		Level1 = (PMMPTE) MI_PTE_LOC(Address);
		
		*Level2 = MmBuildPte(
			MmPhysPageToPFN(V2P(&KiBootstrapPageTables[(i >> 22) * PAGE_SIZE])),
			MM_PROT_READ | MM_PROT_WRITE
		);
		
		*Level1 = MmBuildPte(
			MmPhysPageToPFN(i),
			MM_PROT_READ | MM_PROT_WRITE
		);
	}
	
	// Map the level 2 PTs for the HHDM window.
	Address = MI_FASTMAP_START;
	
	Level2 = (PMMPTE) MI_PTE_LOC(MI_PTE_LOC(Address));
	*Level2 = MmBuildPte(
		MmPhysPageToPFN(V2P(KiHhdmWindowPageTables)),
		MM_PROT_READ | MM_PROT_WRITE
	);
	
	// Map the level 2 PTs for the pool headers.
	for (size_t i = 0; i < MI_POOL_HEADERS_SIZE; i += PAGE_SIZE * PAGE_SIZE / sizeof(MMPTE))
	{
		Address = MI_POOL_HEADERS_START + i;
		
		Level2 = (PMMPTE) MI_PTE_LOC(MI_PTE_LOC(Address));
		*Level2 = MmBuildPte(
			MmPhysPageToPFN(V2P(&KiPoolHeadersPageTables[(i >> 22) * PAGE_SIZE])),
			MM_PROT_READ | MM_PROT_WRITE
		);
	}
}
