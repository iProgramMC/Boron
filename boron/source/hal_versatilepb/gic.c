/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ha/gic.c
	
Abstract:
	This module contains the driver for the GIC (PL190/PL192).
	
Author:
	iProgramInCpp - 31 December 2025
***/
#ifdef TARGET_ARMV6
#include <ke.h>
#include "hali.h"

void HalVpbEndOfInterrupt(int InterruptNumber)
{
	DbgPrint("TODO %s", __func__);
}

void HalVpbRequestIpi(uint32_t LapicId, uint32_t Flags, int Vector)
{
	DbgPrint("TODO %s", __func__);
}

void HalVpbRegisterInterruptHandler(int Irq, void(*Func)())
{
	DbgPrint("TODO %s", __func__);
}

PKREGISTERS HalVpbOnInterruptRequest(PKREGISTERS Registers)
{
	DbgPrint("TODO %s", __func__);
	return Registers;
}

PKREGISTERS HalVpbOnFastInterruptRequest(PKREGISTERS Registers)
{
	DbgPrint("TODO %s", __func__);
	return Registers;
}

int HalVpbGetMaximumInterruptCount()
{
	DbgPrint("TODO %s", __func__);
	return 1;
}

void HalInitPL190()
{
	DbgPrint("TODO %s", __func__);
}

void HalInitTimer()
{
	DbgPrint("TODO %s", __func__);
}

#endif
