/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ha/clock.c
	
Abstract:
	This module contains the platform's clock driver.
	
Author:
	iProgramInCpp - 16 March 2026
***/
#include <mm.h>
#include "hali.h"

#define CLOCK0_MEM_BASE 0x38100000
#define CLOCK1_MEM_BASE 0x3C500000

#define REG(base, rgof) (* (volatile uint32_t*) ((uintptr_t)base + rgof))

#define CLOCK0_CONFIG REG(HalClock0Base, 0x0)
#define CLOCK0_ADJ1   REG(HalClock0Base, 0x8)
#define CLOCK0_ADJ2   REG(HalClock0Base, 0x404)

// NOTE: for now, only supporting the base of what we need
#define CLOCK1_CL2_GATES REG(HalClock1Base, 0x48)
#define CLOCK1_CL3_GATES REG(HalClock1Base, 0x4C)
#define CLOCK1_CL3_SEPARATOR 0x20

void* HalClock0Base;
void* HalClock1Base;

void HalClockSetGateEnabled(uint32_t Gate, bool Enabled)
{
	if (Gate < CLOCK1_CL3_SEPARATOR)
	{
		if (Enabled)
			CLOCK1_CL2_GATES |= 1 << (Gate & 0x1F);
		else
			CLOCK1_CL2_GATES &= ~(1 << (Gate & 0x1F));
	}
	else
	{
		if (Enabled)
			CLOCK1_CL3_GATES |= 1 << (Gate & 0x1F);
		else
			CLOCK1_CL3_GATES &= ~(1 << (Gate & 0x1F));
	}
}

void HalInitClock()
{
	DbgPrint("%s...", __func__);
	
	HalClock0Base = MmMapIoSpace(
		CLOCK0_MEM_BASE,
		PAGE_SIZE,
		MM_PROT_READ | MM_PROT_WRITE | MM_MISC_DISABLE_CACHE,
		POOL_TAG("Clk0")
	);
	
	HalClock1Base = MmMapIoSpace(
		CLOCK1_MEM_BASE,
		PAGE_SIZE,
		MM_PROT_READ | MM_PROT_WRITE | MM_MISC_DISABLE_CACHE,
		POOL_TAG("Clk1")
	);
}

uint64_t HalGetTickCount()
{
	DbgPrint("%s NYI", __func__);
	return 1;
}

uint64_t HalGetTickFrequency()
{
	DbgPrint("%s NYI", __func__);
	return 1000000;
}

bool HalUseOneShotTimer()
{
	DbgPrint("%s NYI", __func__);
	return false;
}
