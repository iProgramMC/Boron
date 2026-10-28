/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ha/chipid.c

Abstract:
	This module contains the platform's chip ID driver.

Licensing:
	Because this driver borrows significantly from OpeniBoot, this driver's
	source code is licensed separately under the GNU General Public License
	version 3.0.  You should have received a copy of the GNU General Public
	License along with this source code file, which is available at the
	`drivers/LICENSE.ARM` path.  Otherwise, view the web page containing the
	license at https://gnu.org/licenses/gpl.html.

Author:
	iProgramInCpp - 28 October 2026
***/
#include "hali.h"
#include <mm.h>
#include "chipid.h"

#define CHIPID_BASE 0x3D100000

#define CPID_SPICLOCKTYPE 0x4
#define CPID_CHIPINFO     0x8

void* HalChipIdBase;

#define REG(Offset) (*(volatile uint32_t*)((uintptr_t)HalChipIdBase + (Offset)))

int HalGetSpiClockTypeChipId()
{
	return (REG(CPID_SPICLOCKTYPE) >> 24) & 0xF;
}

int HalGetSecurityEpochChipId()
{
	return REG(CPID_CHIPINFO) & 1;
}

void HalInitChipId()
{
	HalChipIdBase = MmMapIoSpace(
		CHIPID_BASE,
		PAGE_SIZE,
		MM_PROT_READ | MM_PROT_WRITE | MM_MISC_DISABLE_CACHE,
		POOL_TAG("CpId")
	);

	if (!HalChipIdBase) {
		KeCrash("Hal: Could not map chip ID");
	}
}
