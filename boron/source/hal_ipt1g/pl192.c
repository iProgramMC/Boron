#include "hali.h"

int HalIpodGetMaximumInterruptCount()
{
	DbgPrint("%s NYI", __func__);
	return 1;
}

void HalIpodRegisterInterruptHandler(int Irq, void(*Func)())
{
	DbgPrint("%s NYI", __func__);
}

PKREGISTERS HalIpodOnInterruptRequest(PKREGISTERS Registers)
{
	DbgPrint("%s NYI", __func__);
	return Registers;
}

PKREGISTERS HalIpodOnFastInterruptRequest(PKREGISTERS Registers)
{
	DbgPrint("%s NYI", __func__);
	return Registers;
}

void HalInitPL192()
{
	DbgPrint("%s NYI", __func__);
}

void HalIpodRequestIpi(uint32_t LapicId, uint32_t Flags, int Vector)
{
	DbgPrint("%s NYI", __func__);
}

void HalIpodEndOfInterrupt(int InterruptNumber)
{
	DbgPrint("%s NYI", __func__);
}
