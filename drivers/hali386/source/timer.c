/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ha/timer.c
	
Abstract:
	This module contains the two initialization functions
	(UP-init and MP-init)
	
Author:
	iProgramInCpp - 16 October 2025
***/
#include <ke.h>
#include "hali.h"

// TODO: Decide on a good tick rate.  For slower systems, increased
// time slice time would be better.
#define TIMER_RELOAD_VALUE 4000

// This value is incremented by TIMER_RELOAD_VALUE on every timer interrupt.
static uint64_t PitTicksPassed;
static bool PitInitialized;

HAL_API uint64_t HalGetTickFrequency()
{
	return PIT_TICK_FREQUENCY;
}

HAL_API uint64_t HalGetTickCount()
{
	if (!PitInitialized)
		return 0;
	
	// The PIT decrements from TIMER_RELOAD_VALUE
	uint8_t Low, High;
	Low  = KePortReadByte(PIT_CHANNEL_0_PORT);
	High = KePortReadByte(PIT_CHANNEL_0_PORT);
	
	uint16_t Timer = TIMER_RELOAD_VALUE - (Low | (High << 8));
	return PitTicksPassed + Timer;
}

HAL_API uint64_t HalGetIntTimerFrequency()
{
	return PIT_TICK_FREQUENCY;
}

HAL_API bool HalUseOneShotIntTimer()
{
	return false;
}

HAL_API void HalRequestInterruptInTicks(UNUSED uint64_t Ticks)
{
	DbgPrint("HalRequestInterruptInTicks not implemented");
}

HAL_API uint64_t HalGetIntTimerDeltaTicks()
{
	return TIMER_RELOAD_VALUE;
}

PKREGISTERS HalTimerTick(PKREGISTERS Regs)
{
	DbgPrint("Tick!");
	bool Restore = KeDisableInterrupts();
	
	PitTicksPassed += TIMER_RELOAD_VALUE;
	KePortWriteByte(PIT_CHANNEL_0_PORT, TIMER_RELOAD_VALUE & 0xFF);
	KePortWriteByte(PIT_CHANNEL_0_PORT, TIMER_RELOAD_VALUE >> 8);
	
	KeRestoreInterrupts(Restore);
	
	KeTimerTick();
	
	HalEndOfInterrupt((int) Regs->IntNumber);
	return Regs;
}

void HalInitTimer()
{
	KeRegisterInterrupt(SYSTEM_IRQ(0), HalTimerTick);
	HalPicRegisterInterrupt(SYSTEM_IRQ(0), IPL_CLOCK);
	KeSetInterruptIPL(SYSTEM_IRQ(0), IPL_CLOCK);
	
	bool Restore = KeDisableInterrupts();
	
	KePortWriteByteWait(PIT_COMMAND_PORT,   PIT_CMD_CHANNEL_0 | PIT_CMD_ACCESS_2_BYTES | PIT_CMD_RATE_GENERATOR);
	KePortWriteByteWait(PIT_CHANNEL_0_PORT, TIMER_RELOAD_VALUE & 0xFF);
	KePortWriteByteWait(PIT_CHANNEL_0_PORT, TIMER_RELOAD_VALUE >> 8);
	
	KeRestoreInterrupts(Restore);
}
