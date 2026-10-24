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

int HalTransmitSpi(int Port, const void* Buffer, size_t BufferSize, bool Block, int Unknown)
{
	if (!HalpVftable.TransmitSpi) {
		HalDisplayString("WARNING: HalTransmitSpi not supported");
		return -1;
	}

	return HalpVftable.TransmitSpi(Port, Buffer, BufferSize, Block, Unknown);
}

int HalReceiveSpi(int Port, void* Buffer, size_t BufferSize, bool Block, bool DontTransmitJunk)
{
	if (!HalpVftable.ReceiveSpi) {
		HalDisplayString("WARNING: HalReceiveSpi not supported\n");
		return -1;
	}

	return HalpVftable.ReceiveSpi(Port, Buffer, BufferSize, Block, DontTransmitJunk);
}

int HalTransmitAndReceiveSpi(int Port, const void* TxBuffer, size_t TxBufferLength, void* RxBuffer, size_t RxBufferLength, bool Block)
{
	if (!HalpVftable.TransmitAndReceiveSpi) {
		HalDisplayString("WARNING: HalTransmitAndReceiveSpi not supported\n");
		return -1;
	}

	return HalpVftable.TransmitAndReceiveSpi(Port, TxBuffer, TxBufferLength, RxBuffer, RxBufferLength, Block);
}

void HalSetBaudRateSpi(int Port, int BaudRate, int WordSize, bool IsMaster, bool IsActiveLow, bool LastClockEdgeMissing)
{
	if (!HalpVftable.SetBaudRateSpi) {
		HalDisplayString("WARNING: HalSetBaudRateSpi not supported\n");
		return;
	}

	HalpVftable.SetBaudRateSpi(Port, BaudRate, WordSize, IsMaster, IsActiveLow, LastClockEdgeMissing);
}

#endif
