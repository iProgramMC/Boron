#include "hali.h"

uint64_t HalIpodGetIntTimerFrequency()
{
	DbgPrint("%s NYI", __func__);
	return 1000000;
}

uint64_t HalIpodGetTickCount()
{
	DbgPrint("%s NYI", __func__);
	return 1;
}

uint64_t HalIpodGetTickFrequency()
{
	DbgPrint("%s NYI", __func__);
	return 1000000;
}

uint64_t HalIpodGetIntTimerDeltaTicks()
{
	DbgPrint("%s NYI", __func__);
	return 1000;
}

uint64_t HalIpodGetInterruptDeltaTime()
{
	DbgPrint("%s NYI", __func__);
	return 1000;
}

bool HalIpodUseOneShotTimer()
{
	DbgPrint("%s NYI", __func__);
	return false;
}

bool HalIpodUseOneShotIntTimer()
{
	DbgPrint("%s NYI", __func__);
	return false;
}

void HalIpodRequestInterruptInTicks(uint64_t ticks)
{
	DbgPrint("%s NYI", __func__);
}

void HalInitTimer()
{
	DbgPrint("%s NYI", __func__);
}
