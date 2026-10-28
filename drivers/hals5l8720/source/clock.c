/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ha/clock.c

Abstract:
	This module contains the platform's clock driver.

Licensing:
	Because this driver borrows significantly from OpeniBoot, this driver's
	source code is licensed separately under the GNU General Public License
	version 3.0.  You should have received a copy of the GNU General Public
	License along with this source code file, which is available at the
	`drivers/LICENSE.ARM` path.  Otherwise, view the web page containing the
	license at https://gnu.org/licenses/gpl.html.

Author:
	iProgramInCpp - 28 June 2026
***/
#include <mm.h>
#include "hali.h"
#include "clock.h"
#include "chipid.h"

#define CLOCK0_MEM_BASE 0x3C500000

#define CLOCK_SE1_FREQUENCY 24000000
#define CLOCK_SE0_FREQUENCY 12000000
#define CLOCK_STATIC_INFREQ 27000000

#define REG(base, rgof) (* (volatile uint32_t*) ((uintptr_t)base + rgof))

#define CLOCK0_CONFIG REG(HalClock0Base, 0x0)
#define CLOCK0_ADJ1   REG(HalClock0Base, 0x8)
#define CLOCK0_ADJ2   REG(HalClock0Base, 0x404)

#define CLOCK_CONFIG0  REG(HalClock0Base, 0x00)
#define CLOCK_CONFIG1  REG(HalClock0Base, 0x04)
#define CLOCK_CONFIG2  REG(HalClock0Base, 0x08)
#define CLOCK_PLL0CON  REG(HalClock0Base, 0x20)
#define CLOCK_PLL1CON  REG(HalClock0Base, 0x24)
#define CLOCK_PLL2CON  REG(HalClock0Base, 0x28)
#define CLOCK_PLL0LCNT REG(HalClock0Base, 0x30)
#define CLOCK_PLL1LCNT REG(HalClock0Base, 0x34)
#define CLOCK_PLL2LCNT REG(HalClock0Base, 0x38)
#define CLOCK_PLL_LOCK REG(HalClock0Base, 0x40)
#define CLOCK_PLL_MODE REG(HalClock0Base, 0x44)
#define CLOCK0_GATES_0 REG(HalClock0Base, 0x48)
#define CLOCK0_GATES_1 REG(HalClock0Base, 0x4C)
#define CLOCK0_GATES_2 REG(HalClock0Base, 0x58)
#define CLOCK0_GATES_3 REG(HalClock0Base, 0x68)
#define CLOCK0_GATES_4 REG(HalClock0Base, 0x6C)

// Config 1
#define CLOCK_CLOCK_PLL(x)         ((((x) >> 12) & 0x3) - 1)
#define CLOCK_CLOCK_DIVIDER(x)     (((((x) >> 25) & 0x1F) + 1) << 1)
#define CLOCK_CLOCK_HAS_DIVIDER(x) (((x) >> 30) & 1)

#define CLOCK_MEMORY_PLL(x)         ((((x) >> 12) & 0x3) - 1)
#define CLOCK_MEMORY_DIVIDER(x)     (((((x) >> 1) & 0x1F) + 1) << 1)
#define CLOCK_MEMORY_HAS_DIVIDER(x) (((x) >> 6) & 1)

#define CLOCK_BUS_PLL(x)         ((((x) >> 12) & 0x3) - 1)
#define CLOCK_BUS_DIVIDER(x)     (((((x) >> 17) & 0x1F) + 1) << 1)
#define CLOCK_BUS_HAS_DIVIDER(x) (((x) >> 22) & 1)

#define CLOCK_PERIPHERAL_PLL(x)         ((((x) >> 12) & 0x3) - 1)
#define CLOCK_PERIPHERAL_DIVIDER(x)     (((((x) >> 9) & 0x1F) + 1) << 1)
#define CLOCK_PERIPHERAL_HAS_DIVIDER(x) (((x) >> 14) & 1)

// Config 2
#define CLOCK_UNKNOWN_PLL(x)      ((((x) >> 28) & 0x3) - 1)
#define CLOCK_UNKNOWN_DIVIDER(x)  ((((x) >> 16) & 0xF) + 1)

#define CLOCK_DISPLAY_PLL(x)      ((((x) >> 12) & 0x3) - 1)
#define CLOCK_DISPLAY_DIVIDER1(x) (((x) & 0xF) + 1)
#define CLOCK_DISPLAY_DIVIDER2(x) ((((x) >> 4) & 0xF) + 1)
#define CLOCK_DISPLAY_DIVIDER(x)  (CLOCK_DISPLAY_DIVIDER1(x) * CLOCK_DISPLAY_DIVIDER2(x))

#define CLOCK_PLL_MODE_ON_OFF(x, y)                     (((x) >> (y)) & 0x1)
#define CLOCK_PLL_MODE_USE_SECURITY_EPOCH_IN_FREQ(x, y) (!(((x) >> ((y) + 4)) & 0x1))

#define CLOCK_MDIV(x) (((x) >> 8) & 0x3FF)
#define CLOCK_PDIV(x) (((x) >> 24) & 0x3F)
#define CLOCK_SDIV(x) ((x) & 0x3)

void* HalClock0Base;

static uint32_t HalPllFrequencies[NUM_PLL];
static uint32_t HalFrequencies[FREQ_BASE_COUNT];
static uint32_t HalClockPll;
static uint32_t HalClockSDiv;

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
		&CLOCK0_GATES_0,
		&CLOCK0_GATES_1,
		&CLOCK0_GATES_2,
		&CLOCK0_GATES_3,
		&CLOCK0_GATES_4,
	};

	for (int Reg = 0; Reg < 4; Reg++)
	{
		uint32_t Data = *Regs[Reg];

		if (Enabled)
			Data &= ~HalClockGateTable[Gate][Reg];
		else
			Data |= HalClockGateTable[Gate][Reg];

		*(Regs[Reg]) = Data;
	}
}

uint32_t HalGetBaseClockFrequency()
{
	if (HalGetSecurityEpochChipId() != 0) {
		return CLOCK_SE1_FREQUENCY;
	}

	return CLOCK_SE0_FREQUENCY;
}

void HalCalculatePllFrequencies()
{
	uint32_t Config = CLOCK_CONFIG0;
	uint32_t ClockPll = CLOCK_CLOCK_PLL(Config);

	Config = CLOCK_CONFIG1;

	uint32_t ClockDivider = 1;
	if (CLOCK_CLOCK_HAS_DIVIDER(Config))
		ClockDivider = CLOCK_CLOCK_DIVIDER(Config);

	uint32_t BusPll = ClockPll;
	uint32_t BusDivider = 1;
	if (CLOCK_BUS_HAS_DIVIDER(Config))
		BusDivider = CLOCK_BUS_DIVIDER(Config);

	uint32_t PeripheralPll = ClockPll;
	uint32_t PeripheralDivider = 1;
	if (CLOCK_PERIPHERAL_HAS_DIVIDER(Config))
		PeripheralDivider = CLOCK_PERIPHERAL_DIVIDER(Config);

	uint32_t MemoryPll = ClockPll;
	uint32_t MemoryDivider = 1;
	if (CLOCK_MEMORY_HAS_DIVIDER(Config))
		MemoryDivider = CLOCK_MEMORY_DIVIDER(Config);

	Config = CLOCK_CONFIG2;

	uint32_t UnknownPll = CLOCK_UNKNOWN_PLL(Config);
	uint32_t UnknownDivider = CLOCK_UNKNOWN_DIVIDER(Config);

	uint32_t DisplayPll = CLOCK_DISPLAY_PLL(Config);
	uint32_t DisplayDivider = CLOCK_DISPLAY_DIVIDER(Config);

	HalClockPll = ClockPll;
	for (uint32_t Pll = 0; Pll < NUM_PLL; Pll++)
	{
		uint32_t PllStates = CLOCK_PLL_MODE;
		if (!CLOCK_PLL_MODE_ON_OFF(PllStates, Pll))
		{
			HalPllFrequencies[Pll] = 0;
			DbgPrint("PLL %d: Off.", Pll);
			continue;
		}

		int Mode = CLOCK_PLL_MODE_USE_SECURITY_EPOCH_IN_FREQ(PllStates, Pll);
		uint32_t InputFrequency = Mode ? HalGetBaseClockFrequency() : CLOCK_STATIC_INFREQ;

		uint32_t PllCon;
		switch (Pll)
		{
			default:
			case 0: PllCon = CLOCK_PLL0CON; break;
			case 1: PllCon = CLOCK_PLL1CON; break;
			case 2: PllCon = CLOCK_PLL2CON; break;
		}

		uint64_t AfterMDiv = InputFrequency * CLOCK_MDIV(PllCon);
		uint64_t AfterPDiv = AfterMDiv / CLOCK_PDIV(PllCon);
		uint64_t AfterSDiv;

		if (Pll != ClockPll) {
			AfterSDiv = AfterPDiv / (1ULL << (1 + CLOCK_SDIV(PllCon)));
		}
		else {
			AfterSDiv = AfterPDiv;
			HalClockSDiv = CLOCK_SDIV(PllCon);
		}

		HalPllFrequencies[Pll] = (uint32_t) AfterSDiv;
		DbgPrint("PLL %u: %u hz", Pll, HalPllFrequencies[Pll]);
	}

	HalFrequencies[FREQ_BASE_CLOCK] = HalPllFrequencies[ClockPll] / ClockDivider;
	HalFrequencies[FREQ_BASE_MEMORY] = HalPllFrequencies[MemoryPll] / MemoryDivider;
	HalFrequencies[FREQ_BASE_BUS] = HalPllFrequencies[BusPll] / BusDivider;
	HalFrequencies[FREQ_BASE_UNKNOWN] = HalPllFrequencies[UnknownPll] / UnknownDivider;
	HalFrequencies[FREQ_BASE_PERIPHERAL] = HalFrequencies[PeripheralPll] / PeripheralDivider;
	HalFrequencies[FREQ_BASE_DISPLAY] = HalPllFrequencies[DisplayPll] / DisplayDivider;
	HalFrequencies[FREQ_BASE_FIXED] = (HalGetSecurityEpochChipId() == 1) ? HalGetBaseClockFrequency() : 0;
	HalFrequencies[FREQ_BASE_TIMEBASE] = HalFrequencies[FREQ_BASE_FIXED] / 4;
	HalFrequencies[FREQ_BASE_USB_PHY] = HalGetBaseClockFrequency();

	DbgPrint("Determined frequencies:");
	for (int i = 0; i < FREQ_BASE_COUNT; i++)
	{
		DbgPrint("Freq #%d: %u hz", i, HalFrequencies[i]);
	}

	// OpeniBoot sets the base divisor here... we won't.
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

	// DO NOT DO THIS!  This seems to stop every clock on the system and nothing works anymore
	//CLOCK1_CL2_GATES = 0xFFFFFFFF;
	//CLOCK1_CL3_GATES = 0xFFFFFFFF;

	HalCalculatePllFrequencies();
}

bool HalUseOneShotTimer()
{
	return false;
}
