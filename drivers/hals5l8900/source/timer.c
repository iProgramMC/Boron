/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ha/timer.c
	
Abstract:
	This module contains the platform's timer driver.
	
Author:
	iProgramInCpp - 16 March 2026
***/
#include <mm.h>
#include <string.h>
#include "hali.h"
#include "clock.h"
#include "timer.h"

// Thanks to https://github.com/iDroid-Project/openiBoot.git, and also
// https://github.com/devos50/qemu-ios/tree/ipod_touch_1g for painstakingly
// documenting all the iPod-touch-unique hardware.

static const int HalTimerOffsets[] = {
	TIMER_0_OFFSET,
	TIMER_1_OFFSET,
	TIMER_2_OFFSET,
	TIMER_3_OFFSET,
	TIMER_4_OFFSET,
	TIMER_5_OFFSET,
	TIMER_6_OFFSET,
};

static KINTERRUPT HalTimerInterrupt;
static KSPIN_LOCK HalTimerInterruptLock;

static const uint32_t HalEventTimerConfig =
	TIMER_PRODUCE_INTERRUPTS | TIMER_DIVIDER(TIMER_DIVIDE2) | TIMER_OPTION6;

static void* HalTimerBase;

static void HalTimerInitRtc()
{
	TIMER_UNKREG0 = 0xA;
	TIMER_UNKREG2 = 0xFFFFFFFF;
	TIMER_UNKREG1 = 0xFFFFFFFF;
	TIMER_UNKREG4 = 0xFFFFFFFF;
	TIMER_UNKREG3 = 0xFFFFFFFF;
	TIMER_UNKREG0 = 0x18010;
}

static void HalTimerStop(int TimerIndex)
{
	TIMER_STATE(TimerIndex) = TIMER_STATE_STOP;
}

static void HalTimerConfig(
	int TimerIndex,
	uint32_t Config,
	uint32_t CountBuffer,
	uint32_t CountBuffer2,
	uint32_t Prescaler,
	bool Start
)
{
	bool Disable = KeDisableInterrupts();
	
	// Stop the timer because we're editing it
	TIMER_STATE(TimerIndex) = TIMER_STATE_STOP;
	
	TIMER_CONFIG(TimerIndex) = Config;
	TIMER_COUNT_BUFFER(TimerIndex) = CountBuffer;
	TIMER_COUNT_BUFFER2(TimerIndex) = CountBuffer2;
	TIMER_PRESCALER(TimerIndex) = Prescaler;
	
	TIMER_STATE(TimerIndex) = TIMER_STATE_MANUAL_UPDATE;

	if (Start) {	
		// Go!!
		TIMER_STATE(TimerIndex) = TIMER_STATE_START;
	}
	
	KeRestoreInterrupts(Disable);
}

static void HalTimerSetUnkReg0ForInit()
{
	TIMER_UNKREG0 |= 0xA;
}

void HalTimerHandler(UNUSED PKINTERRUPT Interrupt, UNUSED void* Context)
{
	ASSERT(Interrupt == &HalTimerInterrupt);
	
	uint32_t Stat = TIMER_IRQSTAT;
	
	// signal to the timer controller that we're handling it
	uint32_t Discard = TIMER_IRQLATCH;
	Discard--;
	
	if (Stat & TIMER_SPECIAL_BIT_1)
		TIMER_UNKREG0 |= TIMER_SPECIAL_BIT_1;

	if (Stat & TIMER_SPECIAL_BIT_1)
		TIMER_UNKREG0 |= TIMER_SPECIAL_BIT_2;

	// uhhhhh
	// n.b.  emulator always returns 0xFFFFFFFF
	uint32_t Timer4Flags = Stat >> (8 * (TIMER_COUNT - TIMER_EVENT - 1));
	if (Timer4Flags & (1 << 0))
	{
		KeTimerTick();
	}
	
	TIMER_IRQLATCH = Stat;
}

void HalInitTimer()
{
	HalTimerBase = MmMapIoSpace(
		TIMER1_MEM_BASE,
		PAGE_SIZE * 0x11, // yeah, we also need access to IRQSTAT
		MM_PROT_READ | MM_PROT_WRITE | MM_MISC_DISABLE_CACHE,
		POOL_TAG("Tmr1")
	);
	ASSERT(HalTimerBase);

	HalSetEnabledClockGate(CLOCK_GATE_TIMER, true);
	
	for (int i = 0; i < TIMER_COUNT; i++) {
		HalTimerStop(i);
	}
	
	HalTimerSetUnkReg0ForInit();
	
	HalTimerInitRtc();
	
	bool Restore = KeDisableInterrupts();
	
	KeInitializeInterrupt(
		&HalTimerInterrupt,
		&HalTimerHandler,
		NULL,
		&HalTimerInterruptLock,
		TIMER_IRQ,
		IPL_CLOCK,
		false
	);
	
	KeConnectInterrupt(&HalTimerInterrupt);
	
	// Enable event timer
	HalTimerConfig(
		TIMER_EVENT,
		HalEventTimerConfig,
		TIMER_TICKS_PER_SEC / IRQS_PER_SEC,
		0, // CountBuffer2
		0, // Prescaler
		true
	);
	
	KeRestoreInterrupts(Restore);
}

uint64_t HalGetTickCount()
{
	uint32_t TicksHigh, TicksLow, TicksHigh2;
	
	bool Restore = KeDisableInterrupts();
	if (!HalTimerBase)
	{
		KeRestoreInterrupts(Restore);
		return 0;
	}
	
	do
	{
		TicksHigh  = TIMER_TICKSHIGH;
		TicksLow   = TIMER_TICKSLOW;
		TicksHigh2 = TIMER_TICKSHIGH;
	}
	while (TicksHigh != TicksHigh2);
	
	KeRestoreInterrupts(Restore);
	return ((uint64_t)TicksHigh << 32ULL) | TicksLow;
}

uint64_t HalGetTickFrequency()
{
	return TIMER_TICKS_PER_SEC;
}

uint64_t HalGetIntTimerFrequency()
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

bool HalUseOneShotIntTimer()
{
	return false;
}

void HalRequestInterruptInTicks(uint64_t ticks)
{
	(void) ticks;
	DbgPrint("%s intentionally left blank");
}
