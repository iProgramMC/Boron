#include "hali.h"

uint64_t HalGetIntTimerFrequency()
{
	DbgPrint("%s NYI", __func__);
	return 1000000;
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

uint64_t HalGetIntTimerDeltaTicks()
{
	DbgPrint("%s NYI", __func__);
	return 1000;
}

uint64_t HalGetInterruptDeltaTime()
{
	DbgPrint("%s NYI", __func__);
	return 1000;
}

bool HalUseOneShotTimer()
{
	DbgPrint("%s NYI", __func__);
	return false;
}

bool HalUseOneShotIntTimer()
{
	DbgPrint("%s NYI", __func__);
	return false;
}

void HalRequestInterruptInTicks(uint64_t ticks)
{
	DbgPrint("%s NYI", __func__);
}

void HalInitTimer()
{
	DbgPrint("%s NYI", __func__);
}
