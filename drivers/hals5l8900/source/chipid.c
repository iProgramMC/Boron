#include "hali.h"
#include <mm.h>
#include "chipid.h"

#define CHIPID_BASE 0x3E500000

#define CPID_SPICLOCKTYPE 0x4

void* HalChipIdBase;

#define REG(Offset) (*(volatile uint32_t*)((uintptr_t)HalChipIdBase + (Offset)))

int HalGetSpiClockTypeChipId()
{
	return REG(CPID_SPICLOCKTYPE);
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
