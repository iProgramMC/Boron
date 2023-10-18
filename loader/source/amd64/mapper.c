/*************************************************************\
*                 The Boron Operating System                  *
*                       System Loader                         *
*                                                             *
*              Copyright (C) 2023 iProgramInCpp               *
*                                                             *
*                          mapper.c                           *
\*************************************************************/
#include "mapint.h"
#include "../phys.h"

PPAGEMAP PageMap;

PPAGEMAP GetCurrentPageMap()
{
	uint64_t Return;
	ASM("mov %%cr3, %0":"=a"(Return));
	return (void*) (GetHhdmOffset() + Return);
}

uint64_t* PhysToInt64Array(uintptr_t PhysAddr)
{
	return (uint64_t*)(GetHhdmOffset() + PhysAddr);
}

void InitializeMapper()
{
	PageMap = GetCurrentPageMap();
	
	InitializePmm();
}
