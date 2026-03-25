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
//
// TODO: Fix a bug where the timer sometimes goes backwards?!
#define TIMER_RELOAD_VALUE 15000

// This value is incremented by TIMER_RELOAD_VALUE on every timer interrupt.
static volatile uint64_t PitTicksPassed;
static bool PitInitialized;

HAL_API uint64_t HalGetTickFrequency()
{
	return PIT_TICK_FREQUENCY;
}

HAL_API uint64_t HalGetTickCount()
{
	if (!PitInitialized)
		return 0;

	static uint64_t LastValue = 0;
	bool Restore = KeDisableInterrupts();
	
	uint64_t Highest1, Highest2, Value;
	uint8_t Low, High;
	do
	{
		do
		{
			Highest1 = AtLoad(PitTicksPassed);
			
			KePortWriteByte(PIT_COMMAND_PORT, 0x00); // latch
			Low = KePortReadByte(PIT_CHANNEL_0_PORT);
			High = KePortReadByte(PIT_CHANNEL_0_PORT);
			
			// Allow interrupts through (momentarily)
			KeRestoreInterrupts(Restore);
			ASM("nop":::"memory");
			ASM("nop":::"memory");
			ASM("nop":::"memory");
			ASM("nop":::"memory");
			Restore = KeDisableInterrupts();
			
			Highest2 = AtLoad(PitTicksPassed);
		}
		while (Highest1 != Highest2);
		
		uint16_t ReadIn = (Low | (High << 8));
		uint16_t Timer = TIMER_RELOAD_VALUE - ReadIn;
		
		Value = Highest1 + Timer;
		
		if (Value < LastValue)
		{
			// The current value is lower than the last returned value.  This probably
			// means there's been an extended period of time with interrupts disabled.
			// I know timers can't go backwards (unless they overflow, but they really
			// shouldn't).
			//
			// If interrupts are disabled, just return the last count, since this actually
			// *won't* end up "fixing itself".
			if (Restore == false) {
				return LastValue;
			}
			
			continue;
		}
		
		break;
	}
	while (true);
	
	LastValue = Value;
	KeRestoreInterrupts(Restore);
	return Value;
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
	PitTicksPassed += TIMER_RELOAD_VALUE;
	
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
	PitInitialized = true;
}
