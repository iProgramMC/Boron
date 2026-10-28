/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ha/clock.h

Licensing:
	Because this driver borrows significantly from OpeniBoot, this driver's
	source code is licensed separately under the GNU General Public License
	version 3.0.  You should have received a copy of the GNU General Public
	License along with this source code file, which is available at the
	`drivers/LICENSE.ARM` path.  Otherwise, view the web page containing the
	license at https://gnu.org/licenses/gpl.html.

Author:
	iProgramInCpp - 24 October 2026
***/
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
