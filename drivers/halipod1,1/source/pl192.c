#include "hali.h"

int HalGetMaximumInterruptCount()
{
	DbgPrint("%s NYI", __func__);
	return 1;
}

void HalRegisterInterruptHandler(int Irq, void(*Func)())
{
	DbgPrint("%s NYI", __func__);
}

PKREGISTERS HalOnInterruptRequest(PKREGISTERS Registers)
{
	DbgPrint("%s NYI", __func__);
	return Registers;
}

PKREGISTERS HalOnFastInterruptRequest(PKREGISTERS Registers)
{
	DbgPrint("%s NYI", __func__);
	return Registers;
}

void HalInitPL192()
{
	DbgPrint("%s NYI", __func__);
}

void HalRequestIpi(uint32_t LapicId, uint32_t Flags, int Vector)
{
	DbgPrint("%s NYI", __func__);
}

void HalEndOfInterrupt(int InterruptNumber)
{
	DbgPrint("%s NYI", __func__);
}
