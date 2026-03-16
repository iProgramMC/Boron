/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/armv6/idmap.c
	
Abstract:
	This module implements identity mapping management for
	the armv6 platform.
	
Author:
	iProgramInCpp - 30 December 2025
***/
#include "../mi.h"
#include <mm.h>

#define P2V(n) ((void*)(MI_IDENTMAP_START + (n))
#define V2P(p) ((uintptr_t)(p) - MI_IDENTMAP_START)

// TODO: keep these mnemonics in one place
#define L1PTE(Address) (((uintptr_t)(Address) & ~0x3FF) | MM_ARM_PTEL1_COARSE_PAGE_TABLE)
#define L2PTE(Pfn) MmHardwarePte(MmBuildPte(Pfn, MM_PROT_READ | MM_PROT_WRITE))
/* #define L2PTE(Pfn) (MM_PTE_NEWPFN(Pfn) | MM_ARM_PTEL2_TYPE_SMALLPAGE | MM_PTE_READWRITE) */

#define L1PTE_FLAGS_SEC 0b001010000001110 // Level 1 PTE flags for Section

extern uint32_t KiRootPageTable[];
extern uint32_t KiRootPageTableJibbie[];
extern uint32_t KiRootPageTableDebbie[];
extern uint32_t KiExceptionHandlerTable[];
extern uint32_t KiPoolHeadersPageTables[];

void MiInitializeBaseIdentityMapping()
{
	for (size_t i = 0, j = MI_IDENTMAP_START >> 20;
		i < MI_IDENTMAP_SIZE;
		i += 1024 * 1024, j++)
	{
		uintptr_t Address = MI_IDENTMAP_START_PHYS + i;
		KiRootPageTable[j] = Address | L1PTE_FLAGS_SEC;
	}
	
	// TODO: is this right?
	for (uintptr_t i = 0; i < MI_POOL_HEADERS_SIZE; i += 1024 * 1024)
	{
		uintptr_t Address = MI_POOL_HEADERS_START + i;
		KiRootPageTable[Address >> 20] = ((uintptr_t) &KiPoolHeadersPageTables[i / PAGE_SIZE]) | MM_ARM_PTEL1_COARSE_PAGE_TABLE;
	}
	
	// setup a linear page table view of the root page table.
	MMPFN RootPageTablePfn = (uintptr_t)KiRootPageTable >> 12;
	
	for (int i = 0; i < 4; i++) {
		KiRootPageTableJibbie[i] = L2PTE(RootPageTablePfn + i);
	}
	
	uintptr_t JibbieAddress = (uintptr_t) KiRootPageTableJibbie;
	uintptr_t DebbieAddress = (uintptr_t) KiRootPageTableDebbie;
	
	KiRootPageTableJibbie[4] = L2PTE(MmPhysPageToPFN(JibbieAddress));
	KiRootPageTableJibbie[5] = L2PTE(MmPhysPageToPFN(DebbieAddress));
	KiRootPageTableJibbie[1008] = L2PTE(MmPhysPageToPFN((uintptr_t) KiExceptionHandlerTable));
	
	for (int i = 512; i < 1024; i++) {
		if (KiRootPageTable[i * 4]) {
			MMPFN Pfn = MM_ARM_PTE_PFN(KiRootPageTable[i * 4]);
			KiRootPageTableDebbie[i] = L2PTE(Pfn);
		}
		else {
			KiRootPageTableDebbie[i] = 0;
		}
	}
	
	KeFlushTLB();
	for (int i = 0; i < 4; i++) {
		KiRootPageTable[4088 + i] = L1PTE(DebbieAddress + i * 1024);
		KiRootPageTable[4092 + i] = L1PTE(JibbieAddress + i * 1024);
	}
	
	// Disable Lower Half
	uint32_t* RootPageTableUpperHalf = (uint32_t*)((uintptr_t)KiRootPageTable | 0xC0000000);
	for (size_t i = 0; i < 2048; i++) {
		RootPageTableUpperHalf[i] = 0;
	}
	
	MmFlushTlbUpdates();
	KeSweepIcache();
	KeSweepDcache();
}
