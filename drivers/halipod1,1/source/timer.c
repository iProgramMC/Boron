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

// Thanks to https://github.com/iDroid-Project/openiBoot.git, and also
// https://github.com/devos50/qemu-ios/tree/ipod_touch_1g for painstakingly
// documenting all the iPod-touch-unique hardware.

#define TIMER1_MEM_BASE (0x3E200000)

#define TIMER_COUNT 7
#define TIMER_IRQ   0x7

#define TIMER_STATE_START 1
#define TIMER_STATE_STOP 0
#define TIMER_STATE_MANUAL_UPDATE 2

// not sure why they're assigned so weirdly
#define TIMER_DIVIDE1  4
#define TIMER_DIVIDE2  0
#define TIMER_DIVIDE4  1
#define TIMER_DIVIDE16 2
#define TIMER_DIVIDE64 3

// We will only use a few of these timers.
//
// Timer 1 - Piezo Timer (iPod touch 1G exclusive)
// Timer 4 - Event Timer
// Timer 5 - Vibrator Timer (iPhone 3G exclusive)

// Interestingly the vibrator motor is controlled by timer 5 on the 3G, but
// by AT commands issued to the radio board on the 2G. That's strange..
#define TIMER_EVENT   4
#define TIMER_PIEZO   1
#define TIMER_VIBRATE 5

#define TIMER_0_OFFSET 0x00
#define TIMER_1_OFFSET 0x20
#define TIMER_2_OFFSET 0x40
#define TIMER_3_OFFSET 0x60
// 0x80 skipped?
#define TIMER_4_OFFSET 0xA0
#define TIMER_5_OFFSET 0xC0
#define TIMER_6_OFFSET 0xE0

static const int HalTimerOffsets[] = {
	TIMER_0_OFFSET,
	TIMER_1_OFFSET,
	TIMER_2_OFFSET,
	TIMER_3_OFFSET,
	TIMER_4_OFFSET,
	TIMER_5_OFFSET,
	TIMER_6_OFFSET,
};

#define TIMERREG(rgof)     (* (volatile uint32_t*) ((uintptr_t)HalTimerBase + rgof))
#define TIMERREGN(n, rgof) (* (volatile uint32_t*) ((uintptr_t)HalTimerBase + HalTimerOffsets[n] + rgof))

#define TIMER_CONFIG(n)        TIMERREGN(n, 0x00)
#define TIMER_STATE(n)         TIMERREGN(n, 0x04)
#define TIMER_COUNT_BUFFER(n)  TIMERREGN(n, 0x08)
#define TIMER_COUNT_BUFFER2(n) TIMERREGN(n, 0x0C)
#define TIMER_PRESCALER(n)     TIMERREGN(n, 0x10)
#define TIMER_UNKNOWN3(n)      TIMERREGN(n, 0x14)

#define TIMER_TICKSHIGH  TIMERREG(0x80)
#define TIMER_TICKSLOW   TIMERREG(0x84)
#define TIMER_UNKREG0    TIMERREG(0x88)
#define TIMER_UNKREG1    TIMERREG(0x8C)
#define TIMER_UNKREG2    TIMERREG(0x90)
#define TIMER_UNKREG3    TIMERREG(0x94)
#define TIMER_UNKREG4    TIMERREG(0x98)
#define TIMER_IRQSTAT    TIMERREG(0x10000) // Woah!!
#define TIMER_IRQLATCH   TIMERREG(0xF8)    // 0x118 on the iPodTouch2G

// TIMER_CONFIG
#define TIMER_PRODUCE_INTERRUPTS 0x7000  // INT0_EN | INT1_EN | OVF_EN
#define TIMER_DIVIDER(Divider)   ((Divider) << 8)
#define TIMER_OPTION6            (1 << 6)

#define TIMER_SPECIAL_BIT_1 0x01000000
#define TIMER_SPECIAL_BIT_2 0x02000000

#define TIMER_TICKS_PER_SEC 12000000
#define IRQS_PER_SEC 3

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

	HalClockSetGateEnabled(CLOCK_GATE_TIMER, true);
	
	for (int i = 0; i < TIMER_COUNT; i++) {
		HalTimerStop(i);
	}
	
	HalTimerSetUnkReg0ForInit();
	
	HalTimerInitRtc();
	
	LogMsg("%s:%d", __FILE__, __LINE__);
	bool Restore = KeDisableInterrupts();
	
	LogMsg("%s:%d", __FILE__, __LINE__);
	KeInitializeInterrupt(
		&HalTimerInterrupt,
		&HalTimerHandler,
		NULL,
		&HalTimerInterruptLock,
		TIMER_IRQ,
		IPL_CLOCK,
		false
	);
	
	LogMsg("%s:%d", __FILE__, __LINE__);
	KeConnectInterrupt(&HalTimerInterrupt);
	
	// Enable event timer
	LogMsg("%s:%d", __FILE__, __LINE__);
	HalTimerConfig(
		TIMER_EVENT,
		HalEventTimerConfig,
		TIMER_TICKS_PER_SEC / IRQS_PER_SEC,
		0, // CountBuffer2
		0, // Prescaler
		true
	);
	
	LogMsg("%s:%d", __FILE__, __LINE__);
	KeRestoreInterrupts(Restore);
	
	LogMsg("%s:%d", __FILE__, __LINE__);
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
