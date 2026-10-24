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
#include "clock.h"

#define FREQUENCY_BASE 12000000
#define PLL0_INFREQ_DIV FREQUENCY_BASE
#define PLL1_INFREQ_DIV FREQUENCY_BASE
#define PLL2_INFREQ_DIV FREQUENCY_BASE
#define PLL3_INFREQ_DIV 13500000
#define PLL0_INFREQ_MULT 0x4000
#define PLL1_INFREQ_MULT 0x4000
#define PLL2_INFREQ_MULT 0x4000
#define PLL3_INFREQ_MULT FREQUENCY_BASE

#define CLOCK0_MEM_BASE 0x38100000
#define CLOCK1_MEM_BASE 0x3C500000

#define REG(base, rgof) (* (volatile uint32_t*) ((uintptr_t)base + rgof))

#define CLOCK0_CONFIG REG(HalClock0Base, 0x0)
#define CLOCK0_ADJ1   REG(HalClock0Base, 0x8)
#define CLOCK0_ADJ2   REG(HalClock0Base, 0x404)

// NOTE: for now, only supporting the base of what we need
#define CLOCK1_CONFIG0   REG(HalClock1Base, 0x00)
#define CLOCK1_CONFIG1   REG(HalClock1Base, 0x04)
#define CLOCK1_CONFIG2   REG(HalClock1Base, 0x08)
#define CLOCK1_PLL0CON   REG(HalClock1Base, 0x20)
#define CLOCK1_PLL1CON   REG(HalClock1Base, 0x24)
#define CLOCK1_PLL2CON   REG(HalClock1Base, 0x28)
#define CLOCK1_PLL3CON   REG(HalClock1Base, 0x2C)
#define CLOCK1_PLL_MODE  REG(HalClock1Base, 0x44)
#define CLOCK1_CL2_GATES REG(HalClock1Base, 0x48)
#define CLOCK1_CL3_GATES REG(HalClock1Base, 0x4C)
#define CLOCK1_CL3_SEPARATOR 0x20

#define CLOCK1_CLOCK_PLL(x)         (((x) >> 12) & 0x3)
#define CLOCK1_CLOCK_DIVIDER(x)     (((x) & 0xF) + 1)
#define CLOCK1_CLOCK_HAS_DIVIDER(x) (((x) >> 8) & 1)

#define CLOCK1_MEMORY_PLL(x)         (((x) >> 12) & 0x3)
#define CLOCK1_MEMORY_DIVIDER(x)     ((((x) >> 16) & 0xF) + 1)
#define CLOCK1_MEMORY_HAS_DIVIDER(x) (((x) >> 24) & 1)

// TODO: is this really supposed to be the same as the MEMORY config?
#define CLOCK1_BUS_PLL(x)         (((x) >> 12) & 0x3)
#define CLOCK1_BUS_DIVIDER(x)     ((((x) >> 16) & 0xF) + 1)
#define CLOCK1_BUS_HAS_DIVIDER(x) (((x) >> 24) & 1)

#define CLOCK1_UNKNOWN_PLL(x)         (((x) >> 12) & 0x3)
#define CLOCK1_UNKNOWN_DIVIDER1(x)    (((x) & 0xF) + 1)
#define CLOCK1_UNKNOWN_DIVIDER2(x)    ((((x) >> 4) & 0xF) + 1)
#define CLOCK1_UNKNOWN_DIVIDER(x)     (CLOCK1_UNKNOWN_DIVIDER1(x) * CLOCK1_UNKNOWN_DIVIDER2(x))
#define CLOCK1_UNKNOWN_HAS_DIVIDER(x) (((x) >> 8) & 1)

#define CLOCK1_PERIPHERAL_DIVIDER(x) (((x) >> 20) & 0x3)

#define CLOCK1_DISPLAY_PLL(x)         (((x) >> 28) & 0x3)
#define CLOCK1_DISPLAY_DIVIDER(x)     (((x) >> 16) & 0xF)
#define CLOCK1_DISPLAY_HAS_DIVIDER(x) (((x) >> 24) & 0x1)

#define CLOCK1_PLL_MODE_ON_OFF(x, y)       (((x) >> (y)) & 0x1)
#define CLOCK1_PLL_MODE_DIVIDER_MODE(x, y) (((x) >> ((y) + 4)) & 0x1)
#define CLOCK1_PLL_MODE_DIVIDE   1
#define CLOCK1_PLL_MODE_MULTIPLY 0

#define CLOCK1_MDIV(x) (((x) >> 8) & 0x3FF)
#define CLOCK1_PDIV(x) (((x) >> 24) & 0x3F)
#define CLOCK1_SDIV(x) ((x) & 0x3)

void* HalClock0Base;
void* HalClock1Base;

static uint32_t HalPllFrequencies[NUM_PLL];
static uint32_t HalFrequencies[FREQ_BASE_COUNT];
static uint32_t HalClockPll;
static uint32_t HalClockSDiv;

void HalSetEnabledClockGate(int Gate, bool Enabled)
{
	if (Gate < CLOCK1_CL3_SEPARATOR)
	{
		if (Enabled)
			CLOCK1_CL2_GATES &= ~(1 << (Gate & 0x1F));
		else
			CLOCK1_CL2_GATES |= 1 << (Gate & 0x1F);
	}
	else
	{
		if (Enabled)
			CLOCK1_CL3_GATES &= ~(1 << (Gate & 0x1F));
		else
			CLOCK1_CL3_GATES |= 1 << (Gate & 0x1F);
	}
}

void HalCalculatePllFrequencies()
{
	uint32_t Config = CLOCK1_CONFIG0;

	uint32_t ClockPll = CLOCK1_CLOCK_PLL(Config);
	uint32_t ClockDivisor = 1;
	if (CLOCK1_CLOCK_HAS_DIVIDER(Config))
		ClockDivisor = CLOCK1_CLOCK_DIVIDER(Config);

	uint32_t MemoryPll = CLOCK1_MEMORY_PLL(Config);
	uint32_t MemoryDivisor = 1;
	if (CLOCK1_MEMORY_HAS_DIVIDER(Config))
		MemoryDivisor = CLOCK1_MEMORY_DIVIDER(Config);

	Config = CLOCK1_CONFIG1;

	uint32_t BusPll = CLOCK1_BUS_PLL(Config);
	uint32_t BusDivisor = 1;
	if (CLOCK1_BUS_HAS_DIVIDER(Config))
		BusDivisor = CLOCK1_BUS_DIVIDER(Config);

	uint32_t UnknownPll = CLOCK1_UNKNOWN_PLL(Config);
	uint32_t UnknownDivisor = 1;
	if (CLOCK1_UNKNOWN_HAS_DIVIDER(Config))
		UnknownDivisor = CLOCK1_UNKNOWN_DIVIDER(Config);

	uint32_t PeripheralFactor = CLOCK1_PERIPHERAL_DIVIDER(Config);

	Config = CLOCK1_CONFIG2;

	// NOTE: OpeniBoot swaps DISPLAYDIVIDER and DISPLAYHASDIVIDER...?
	// I'll assume this is incorrect...
	uint32_t DisplayPll = CLOCK1_DISPLAY_PLL(Config);
	uint32_t DisplayDivisor = 1;
	if (CLOCK1_DISPLAY_HAS_DIVIDER(Config))
		DisplayDivisor = CLOCK1_DISPLAY_DIVIDER(Config) + 1;

	HalClockPll = ClockPll;
	for (uint32_t Pll = 0; Pll < NUM_PLL; Pll++)
	{
		uint32_t PllStates = CLOCK1_PLL_MODE;
		if (!CLOCK1_PLL_MODE_ON_OFF(PllStates, Pll))
		{
			HalPllFrequencies[Pll] = 0;
			DbgPrint("PLL %d: Off.", Pll);
			continue;
		}

		int DividerMode = CLOCK1_PLL_MODE_DIVIDER_MODE(PllStates, Pll);
		int DividerModeInputFrequency = DividerMode;

		uint32_t InputFrequency, InputFrequencyMult, PllCon;
		switch (Pll)
		{
			case 0: {
				InputFrequency = PLL0_INFREQ_DIV * 2;
				PllCon = CLOCK1_PLL0CON;
				InputFrequencyMult = PLL0_INFREQ_MULT * 2;
				break;
			}
			case 1: {
				InputFrequency = PLL1_INFREQ_DIV * 2;
				PllCon = CLOCK1_PLL1CON;
				InputFrequencyMult = PLL1_INFREQ_MULT * 2;
				break;
			}
			case 2: {
				InputFrequency = PLL2_INFREQ_DIV * 2;
				PllCon = CLOCK1_PLL2CON;
				InputFrequencyMult = PLL2_INFREQ_MULT * 2;
				break;
			}
			case 3: {
				InputFrequency = PLL3_INFREQ_DIV * 2;
				PllCon = CLOCK1_PLL3CON;
				InputFrequencyMult = PLL3_INFREQ_MULT * 2;
				DividerMode = CLOCK1_PLL_MODE_DIVIDE;
				break;
			}
		}

		if (DividerModeInputFrequency == CLOCK1_PLL_MODE_MULTIPLY) {
			InputFrequency = InputFrequencyMult;
		}

		uint64_t AfterMDiv = InputFrequency * CLOCK1_MDIV(PllCon);
		uint64_t AfterPDiv;

		if (DividerMode == CLOCK1_PLL_MODE_DIVIDE) {
			AfterPDiv = AfterMDiv / CLOCK1_PDIV(PllCon);
		}
		else {
			AfterPDiv = AfterMDiv * CLOCK1_PDIV(PllCon);
		}

		uint64_t AfterSDiv;
		if (Pll != ClockPll) {
			AfterSDiv = AfterPDiv / (1ULL << CLOCK1_SDIV(PllCon));
		}
		else {
			AfterSDiv = AfterPDiv;
			HalClockSDiv = CLOCK1_SDIV(PllCon);
		}

		HalPllFrequencies[Pll] = (uint32_t) AfterSDiv;
		DbgPrint("PLL %u: %u hz\n", Pll, HalPllFrequencies[Pll]);
	}

	HalFrequencies[FREQ_BASE_CLOCK] = HalPllFrequencies[ClockPll] / ClockDivisor;
	HalFrequencies[FREQ_BASE_MEMORY] = HalPllFrequencies[MemoryPll] / MemoryDivisor;
	HalFrequencies[FREQ_BASE_BUS] = HalPllFrequencies[BusPll] / BusDivisor;
	HalFrequencies[FREQ_BASE_UNKNOWN] = HalPllFrequencies[UnknownPll] / UnknownDivisor;
	HalFrequencies[FREQ_BASE_PERIPHERAL] = HalFrequencies[FREQ_BASE_BUS] / (1 << PeripheralFactor);
	HalFrequencies[FREQ_BASE_DISPLAY] = HalPllFrequencies[DisplayPll] / DisplayDivisor;
	HalFrequencies[FREQ_BASE_FIXED] = FREQUENCY_BASE * 2;
	HalFrequencies[FREQ_BASE_TIMEBASE] = FREQUENCY_BASE / 2;

	DbgPrint("Determined frequencies:");
	for (int i = 0; i < FREQ_BASE_COUNT; i++)
	{
		DbgPrint("Freq #%d: %u hz", i, HalFrequencies[i]);
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

	// DO NOT DO THIS!  This seems to stop every clock on the system and nothing works anymore
	//CLOCK1_CL2_GATES = 0xFFFFFFFF;
	//CLOCK1_CL3_GATES = 0xFFFFFFFF;

	HalCalculatePllFrequencies();
}

bool HalUseOneShotTimer()
{
	return false;
}

uint32_t HalGetClockFrequency(int FrequencyBase)
{
	return HalFrequencies[FrequencyBase];
}
