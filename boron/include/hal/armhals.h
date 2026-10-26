/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	hal/armhals.h

Abstract:
	This header file contains the definitions for the ARM-specific
	HAL callbacks.

Author:
	iProgramInCpp - 6 May 2026
***/
#pragma once

#ifndef TARGET_ARM
#error Do not include this!
#endif

#define APPLE_PL192_MAX_INTERRUPTS_TOTAL 64
#define APPLE_IRQ_NUMBER_GPIO_TO_NORMAL(x) ((x) + APPLE_PL192_MAX_INTERRUPTS_TOTAL)
#define APPLE_IRQ_NUMBER_NORMAL_TO_GPIO(x) ((x) - APPLE_PL192_MAX_INTERRUPTS_TOTAL)

int HalGetMaximumInterruptCount();

void HalOnUpdateIpl(KIPL NewIpl, KIPL OldIpl);

void HalVicRegisterInterrupt(int Vector, KIPL Ipl);

void HalVicDeregisterInterrupt(int Vector, KIPL Ipl);

PKREGISTERS HalOnInterruptRequest(PKREGISTERS Registers);

PKREGISTERS HalOnFastInterruptRequest(PKREGISTERS Registers);

// SCOPE CREEP:  These really shouldn't be added in the HAL-kernel interface.
// The kernel doesn't use them, but other drivers might.
//
// Really, a more proper solution would be to allow drivers to link against the HAL.

// APPLE HAL START
#define HAL_SPI_CLOCKSOURCE_PCLK 0 // Peripheral Clock
#define HAL_SPI_CLOCKSOURCE_NCLK 1 // Fixed Clock

void HalSetEnabledClockGate(int ClockGateId, bool Enabled);

void HalRegisterGpioInterrupt(int InterruptNumber, bool TriggerEdge, bool Level, bool FlipLevel);

bool HalGetPinStateGpio(int Port);

void HalFselGpio(int Port, int Bits);

void HalEnableGpioInterrupt(int InterruptNumber);

void HalDisableGpioInterrupt(int InterruptNumber);

void HalSetInputPinGpio(int Port);

void HalSetOutputPinGpio(int Port, int Bit);

int HalTransmitSpi(int Port, const void* Buffer, size_t BufferSize, bool Block, int Unknown);

int HalReceiveSpi(int Port, void* Buffer, size_t BufferSize, bool Block, bool DontTransmitJunk);

int HalTransmitAndReceiveSpi(int Port, const void* TxBuffer, size_t TxBufferLength, void* RxBuffer, size_t RxBufferLength, bool Block);

void HalSetBaudRateSpi(int Port, int BaudRate, int WordSize, bool IsMaster, bool IsActiveLow, bool LastClockEdgeMissing);
// APPLE HAL END
