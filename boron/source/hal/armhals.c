/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	hal/armhals.c
	
Abstract:
	This module contains the implementation of the ARM-specific
	HAL dispatch functions.
	
Author:
	iProgramInCpp - 6 May 2026
***/
#include <hal.h>
#include <ke.h>

extern HAL_VFTABLE HalpVftable;

#ifdef TARGET_ARM

int HalGetMaximumInterruptCount()
{
	return HalpVftable.GetMaximumInterruptCount();
}

void HalOnUpdateIpl(KIPL NewIpl, KIPL OldIpl)
{
	HalpVftable.OnUpdateIpl(NewIpl, OldIpl);
}

void HalVicRegisterInterrupt(int Vector, KIPL Ipl)
{
	HalpVftable.VicRegisterInterrupt(Vector, Ipl);
}

void HalVicDeregisterInterrupt(int Vector, KIPL Ipl)
{
	HalpVftable.VicDeregisterInterrupt(Vector, Ipl);
}

PKREGISTERS HalOnInterruptRequest(PKREGISTERS Registers)
{
	return HalpVftable.OnInterruptRequest(Registers);
}

PKREGISTERS HalOnFastInterruptRequest(PKREGISTERS Registers)
{
	return HalpVftable.OnFastInterruptRequest(Registers);
}

void HalSetEnabledClockGate(int ClockGateId, bool Enabled)
{
	return HalpVftable.SetEnabledClockGate(ClockGateId, Enabled);
}

void HalRegisterGpioInterrupt(int InterruptNumber, bool TriggerEdge, bool Level, bool FlipLevel)
{
	return HalpVftable.RegisterGpioInterrupt(InterruptNumber, TriggerEdge, Level, FlipLevel);
}

bool HalGetPinStateGpio(int Port)
{
	return HalpVftable.GetPinStateGpio(Port);
}

void HalFselGpio(int Port, int Bits)
{
	return HalpVftable.FselGpio(Port, Bits);
}

void HalEnableGpioInterrupt(int InterruptNumber)
{
	return HalpVftable.EnableGpioInterrupt(InterruptNumber);
}

void HalDisableGpioInterrupt(int InterruptNumber)
{
	return HalpVftable.DisableGpioInterrupt(InterruptNumber);
}

void HalSetInputPinGpio(int Port)
{
	return HalpVftable.SetInputPinGpio(Port);
}

void HalSetOutputPinGpio(int Port, int Bit)
{
	return HalpVftable.SetOutputPinGpio(Port, Bit);
}

#endif
