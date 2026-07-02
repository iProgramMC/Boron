/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ha/clock.c
	
Abstract:
	This module contains the platform's clock driver.
	
Author:
	iProgramInCpp - 28 June 2026
***/
#include <mm.h>
#include "hali.h"

#define CLOCK0_MEM_BASE 0x3C500000

#define REG(base, rgof) (* (volatile uint32_t*) ((uintptr_t)base + rgof))

#define CLOCK0_CONFIG REG(HalClock0Base, 0x0)
#define CLOCK0_ADJ1   REG(HalClock0Base, 0x8)
#define CLOCK0_ADJ2   REG(HalClock0Base, 0x404)

// NOTE: for now, only supporting the base of what we need
#define CLOCK1_CL2_GATES REG(HalClock0Base, 0x48)
#define CLOCK1_CL3_GATES REG(HalClock0Base, 0x4C)
#define CLOCK1_CL3_SEPARATOR 0x20

#define CLOCK1_GATES_0 REG(HalClock0Base, 0x48)
#define CLOCK1_GATES_1 REG(HalClock0Base, 0x4C)
#define CLOCK1_GATES_2 REG(HalClock0Base, 0x58)
#define CLOCK1_GATES_3 REG(HalClock0Base, 0x68)
#define CLOCK1_GATES_4 REG(HalClock0Base, 0x6C)

void* HalClock0Base;

const uint32_t HalClockGateTable[][5] =
{
	/* 0x0 */  { 0x00000080,   0x0,        0x0,        0x0,        0x0  },
	/* 0x1 */  { 0x0,          0x00004000, 0x0,        0x0,        0x0  },
	/* 0x2 */  { 0x00004000,   0x0,        0x0,        0x00004E00, 0x0  },
	/* 0x3 */  { 0x00000800,   0x0,        0x0,        0x0,        0x0  },
	/* 0x4 */  { 0x00001000,   0x0,        0x0,        0x0,        0x0  },
	/* 0x5 */  { 0x00000220,   0x0,        0x0,        0x0,        0x0  },
	/* 0x6 */  { 0x0,          0x00001000, 0x0,        0x0,        0x0  },
	/* 0x7 */  { 0x0,          0x00000010, 0x0,        0x0,        0x00000800 },
	/* 0x8 */  { 0x0,          0x00000040, 0x0,        0x0,        0x00001000 },
	/* 0x9 */  { 0x00000002,   0x0,  0x0,  0x0,        0x00010000 },
	/* 0xA */  { 0x0,          0x00080000, 0x0,        0x0,        0x0  },
	/* 0xB */  { 0x0,          0x00002000, 0x0,        0x0,        0x0  },
	/* 0xC */  { 0x00000400,   0x0,        0x0,        0x0,        0x0  },
	/* 0xD */  { 0x00000001,   0x0,        0x0,        0x0,        0x0  },
	/* 0xE */  { 0x0,          0x00000004, 0x0,        0x0,        0x00002000 },
	/* 0xF */  { 0x0,          0x00000800, 0x0,        0x0,        0x00004000 },
	/* 0x10 */ { 0x0,          0x00008000, 0x0,        0x0,        0x00008000 },
	/* 0x11 */ { 0x0,          0x0,        0x00000002, 0x0,        0x00080000 },
	/* 0x12 */ { 0x0,          0x0,        0x00000010, 0x0,        0x00100000 },
	/* 0x13 */ { 0x0,          0x1F800020, 0x00000060, 0x0,        0x00C0007F },
	/* 0x14 */ { 0x0,          0x00000200, 0x0,        0x0,        0x00000080 },
	/* 0x15 */ { 0x0,          0x20000000, 0x0,        0x0,        0x00000100 },
	/* 0x16 */ { 0x0,          0x40000000, 0x0,        0x0,        0x00000200 },
	/* 0x17 */ { 0x0,          0x80000000, 0x0,        0x0,        0x00000400 },
	/* 0x18 */ { 0x00000004,   0x0,        0x0,        0x0,        0x0  },
	/* 0x19 */ { 0x0,          0x00000008, 0x0,        0x0,        0x0  },
	/* 0x1A */ { 0x0,          0x0,        0x00000001, 0x0,        0x0  },
	/* 0x1B */ { 0x00000220,   0x0,        0x0,        0x0,        0x0  },
	/* 0x1C */ { 0x00000220,   0x0,        0x0,        0x0,        0x0 }
};

void HalSetEnabledClockGate(int Gate, bool Enabled)
{
	volatile uint32_t* Regs[] = {
		&CLOCK1_GATES_0,
		&CLOCK1_GATES_1,
		&CLOCK1_GATES_2,
		&CLOCK1_GATES_3,
		&CLOCK1_GATES_4,
	};
	
	for (int Reg = 0; Reg < 4; Reg++)
	{
	LogMsg("%s (%s:%d)...", __func__, __FILE__, __LINE__);
		uint32_t Data = *Regs[Reg];
	LogMsg("%s (%s:%d)...", __func__, __FILE__, __LINE__);
		
		if (Enabled)
			Data &= ~HalClockGateTable[Gate][Reg];
		else
			Data |= HalClockGateTable[Gate][Reg];
		
	LogMsg("%s (%s:%d)...", __func__, __FILE__, __LINE__);
		*(Regs[Reg]) = Data;
	LogMsg("%s (%s:%d)...", __func__, __FILE__, __LINE__);
	}
}

void HalInitClock()
{
	DbgPrint("%s...", __func__);
	
	LogMsg("%s (%s:%d)...", __func__, __FILE__, __LINE__);
	HalClock0Base = MmMapIoSpace(
		CLOCK0_MEM_BASE,
		PAGE_SIZE,
		MM_PROT_READ | MM_PROT_WRITE | MM_MISC_DISABLE_CACHE,
		POOL_TAG("Clk0")
	);
	
	LogMsg("%s (%s:%d)...  %p", __func__, __FILE__, __LINE__, HalClock0Base);
	// DO NOT DO THIS!  This seems to stop every clock on the system and nothing works anymore
	//CLOCK1_CL2_GATES = 0xFFFFFFFF;
	//CLOCK1_CL3_GATES = 0xFFFFFFFF;
}

bool HalUseOneShotTimer()
{
	return false;
}
