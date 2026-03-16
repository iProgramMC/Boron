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

static int colors[] = {
	0b1111100000000000, // 0-RED
	0b1111101111100000, // 1-ORANGE
	0b1111111111100000, // 2-YELLOW
	0b1111100000011111, // 3-GREENYELLOW
	0b0000011111100000, // 4-GREEN
	0b1111100000011111, // 5-GREENBLUE
	0b0000011111111111, // 6-CYAN
	0b0000001111111111, // 7-TURQUOISE
	0b0000000000011111, // 8-BLUE
	0b1111100000011111, // 9-MAGENTA
	0b1111100000000000, // 10-RED
	0b1111101111100000, // 11-ORANGE
	0b1111111111100000, // 12-YELLOW
	0b0111111111100000, // 13-GREENYELLOW
	0b0000011111100000, // 14-GREEN
	0b0000011111101111, // 15-GREENBLUE//skipped this one!?
	0b0000011111111111, // 16-CYAN
	0b0000001111111111, // 17-TURQUOISE
	0b0000000000011111, // 18-BLUE
	0b1111100000011111, // 19-MAGENTA
};

void Marker3(int i)
{
	int* a;
	if (KeGetCurrentPageTable() == (uintptr_t) &KiRootPageTable) {
		a = (int*)(0x400000+320*4*200);
	}
	else {
		a = (int*)(0xCFD00000+320*4*200);
	}
	
	int pattern = colors[i] | (colors[i] << 16);
	a[i * 4 + 3] = a[i * 4 + 2] = a[i * 4 + 1] = a[i * 4 + 0] = pattern;
	
	//const int stopAt = 7;
	//if (stopAt <= i) {
	//	while (true) {
	//		ASM("wfi");
	//	}
	//}
}

void MiInitializeBaseIdentityMapping()
{
	Marker3(0);
	for (size_t i = 0, j = MI_IDENTMAP_START >> 20;
		i < MI_IDENTMAP_SIZE;
		i += 1024 * 1024, j++)
	{
		uintptr_t Address = MI_IDENTMAP_START_PHYS + i;
		KiRootPageTable[j] = Address | L1PTE_FLAGS_SEC;
	}
	
	Marker3(1);
	// TODO: is this right?
	for (uintptr_t i = 0; i < MI_POOL_HEADERS_SIZE; i += 1024 * 1024)
	{
		uintptr_t Address = MI_POOL_HEADERS_START + i;
		KiRootPageTable[Address >> 20] = ((uintptr_t) &KiPoolHeadersPageTables[i / PAGE_SIZE]) | MM_ARM_PTEL1_COARSE_PAGE_TABLE;
	}
	
	Marker3(2);
	// setup a linear page table view of the root page table.
	MMPFN RootPageTablePfn = (uintptr_t)KiRootPageTable >> 12;
	
	Marker3(3);
	for (int i = 0; i < 4; i++) {
		KiRootPageTableJibbie[i] = L2PTE(RootPageTablePfn + i);
	}
	
	Marker3(4);
	uintptr_t JibbieAddress = (uintptr_t) KiRootPageTableJibbie;
	uintptr_t DebbieAddress = (uintptr_t) KiRootPageTableDebbie;
	
	Marker3(5);
	KiRootPageTableJibbie[4] = L2PTE(MmPhysPageToPFN(JibbieAddress));
	KiRootPageTableJibbie[5] = L2PTE(MmPhysPageToPFN(DebbieAddress));
	KiRootPageTableJibbie[1008] = L2PTE(MmPhysPageToPFN((uintptr_t) KiExceptionHandlerTable));
	
	Marker3(6);
	for (int i = 512; i < 1024; i++) {
		if (KiRootPageTable[i * 4]) {
			MMPFN Pfn = MM_ARM_PTE_PFN(KiRootPageTable[i * 4]);
			KiRootPageTableDebbie[i] = L2PTE(Pfn);
		}
		else {
			KiRootPageTableDebbie[i] = 0;
		}
	}
	
	Marker3(7);
	KeFlushTLB();
	Marker3(8);
	for (int i = 0; i < 4; i++) {
		KiRootPageTable[4088 + i] = L1PTE(DebbieAddress + i * 1024);
		KiRootPageTable[4092 + i] = L1PTE(JibbieAddress + i * 1024);
		Marker3(9+i);
	}
	
	Marker3(13);
	MmFlushTlbUpdates();
	Marker3(14);
	KeSweepIcache();
	Marker3(15);
	KeSweepDcache();
	Marker3(16);
}
