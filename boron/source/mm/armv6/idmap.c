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

#define P2V(n) ((void*)(MI_IDENTMAP_START + (n))
#define V2P(p) ((uintptr_t)(p) - MI_IDENTMAP_START)

#define L1PTE_FLAGS_SEC 0b111010000001110 // Level 1 PTE flags for Section
#define L1PTE_FLAGS_CPT 0b111010000001101 // Level 1 PTE flags for Coarse Page Table

extern uint32_t KiRootPageTable[];

void MiInitializeBaseIdentityMapping()
{
	for (size_t i = 0, j = MI_IDENTMAP_START >> 20;
		i < MI_IDENTMAP_SIZE;
		i += 1024 * 1024, j++)
	{
		uintptr_t Address = MI_IDENTMAP_START + i;
		KiRootPageTable[i] = L1PTE_FLAGS_SEC | Address;
	}
}
