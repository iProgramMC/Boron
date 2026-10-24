#pragma once

#define NUM_PLL 4

enum
{
	FREQ_BASE_CLOCK,
	FREQ_BASE_MEMORY,
	FREQ_BASE_BUS,
	FREQ_BASE_PERIPHERAL,
	FREQ_BASE_UNKNOWN,
	FREQ_BASE_DISPLAY,
	FREQ_BASE_FIXED,
	FREQ_BASE_TIMEBASE,
	FREQ_BASE_USB_PHY,
	FREQ_BASE_NAND,
	FREQ_BASE_COUNT
};

uint32_t HalGetClockFrequency(int FrequencyBase);

void HalSetEnabledClockGate(int Gate, bool Enabled);

void HalInitClock();
