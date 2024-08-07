/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ha/apic.c
	
Abstract:
	This module contains the support routines for the APIC.
	
Author:
	iProgramInCpp - 14 September 2023
***/

#include "hali.h"
#include <mm.h>
#include <ke.h>
#include "apic.h"
#include "acpi.h"
#include "hpet.h"
#include "pit.h"
#include "tsc.h"

#define APIC_TIMER_MODE_ONESHOT   (0b00 << 17)
#define APIC_TIMER_MODE_PERIODIC  (0b01 << 17) // not used right now, but may be needed.
#define APIC_TIMER_MODE_TSCDEADLN (0b10 << 17)

#define APIC_LVT_INT_MASKED (0x10000)

#define IA32_APIC_BASE_MSR (0x1B)

static int HalpVectorApicTimer, HalpVectorSpurious;

enum
{
	APIC_REG_ID            = 0x20,
	APIC_REG_VER           = 0x30,
	APIC_REG_TASK_PRIORITY = 0x80,
	APIC_REG_ARB_PRIORITY  = 0x90,
	APIC_REG_PROC_PRIORITY = 0xA0,
	APIC_REG_EOI           = 0xB0,
	APIC_REG_REMOTE_READ   = 0xC0,
	APIC_REG_LOGICAL_DEST  = 0xD0,
	APIC_REG_DEST_FORMAT   = 0xE0,
	APIC_REG_SPURIOUS      = 0xF0,
	APIC_REG_ISR_START     = 0x100, // 0x100 - 0x170
	APIC_REG_TRIG_MODE     = 0x180, // 0x180 - 0x1F0
	APIC_REG_IRQ           = 0x200, // 0x200 - 0x270
	APIC_REG_ERROR_STAT    = 0x280,
	APIC_REG_LVT_CMCI      = 0x2F0,
	APIC_REG_ICR0          = 0x300,
	APIC_REG_ICR1          = 0x310,
	APIC_REG_LVT_TIMER     = 0x320,
	APIC_REG_LVT_THERMAL   = 0x330,
	APIC_REG_LVT_PERFMON   = 0x340,
	APIC_REG_LVT_LINT0     = 0x350,
	APIC_REG_LVT_LINT1     = 0x360,
	APIC_REG_LVT_ERROR     = 0x370,
	APIC_REG_TMR_INIT_CNT  = 0x380,
	APIC_REG_TMR_CURR_CNT  = 0x390,
	APIC_REG_TMR_DIV_CFG   = 0x3E0,
};

enum
{
	APIC_ICR0_DELIVERY_STATUS = (1 << 12),
};

enum
{
	APIC_ICR0_SINGLE           = (0 << 18),
	APIC_ICR0_SELF             = (1 << 18),
	APIC_ICR0_BROADCAST        = (2 << 18),
	APIC_ICR0_BROADCAST_OTHERS = (3 << 18),
};

static uintptr_t ApicGetPhysicalBase()
{
	// Read the specific MSR.
	/*uintptr_t msr = (uintptr_t)KeGetMSR(IA32_APIC_BASE_MSR);
	
	return msr & 0x0000'000F'FFFF'F000;
	*/
	return 0xFEE00000; // really I think it's fine if we hardcode it
}

static void ApicWriteRegister(uint32_t reg, uint32_t value)
{
	volatile uint32_t* ApicAddr = MmGetHHDMOffsetAddr(ApicGetPhysicalBase());
	
	ApicAddr[reg >> 2] = value;
}

static uint32_t ApicReadRegister(uint32_t reg)
{
	volatile uint32_t* ApicAddr = MmGetHHDMOffsetAddr(ApicGetPhysicalBase());
	
	return ApicAddr[reg >> 2];
}

static void ApicEndOfInterrupt()
{
	ApicWriteRegister(APIC_REG_EOI, 0);
}

bool HalIsApicAvailable()
{
	// Use the CPUID instruction to check if the APIC is present on the current CPU
	uint32_t eax, edx;
	ASM("cpuid":"=d"(edx),"=a"(eax):"a"(1));
	
	return edx & (1 << 9);
}

static void ApicSendIpi(uint32_t lapicID, int vector, bool broadcast, bool self)
{
	// wait until our current IPI request is finished
	while (ApicReadRegister(APIC_REG_ICR0) & APIC_ICR0_DELIVERY_STATUS)
		KeSpinningHint();
	
	// write the destination CPU's lapic ID to the ICR1 register
	ApicWriteRegister(APIC_REG_ICR1, lapicID << 24);
	
	// write the interrupt vector to the ICR0 register
	int mode = APIC_ICR0_SINGLE;
	if (broadcast)
	{
		mode = APIC_ICR0_BROADCAST_OTHERS;
		if (self)
			mode = APIC_ICR0_BROADCAST;
	}
	else if (self)
	{
		mode = APIC_ICR0_SELF;
	}
	
	ApicWriteRegister(APIC_REG_ICR0, mode | vector);
}

// Interface code.
void HalSendIpi(uint32_t Processor, int Vector)
{
	ApicSendIpi(Processor, Vector, false, false);
}

void HalBroadcastIpi(int Vector, bool IncludeSelf)
{
	ApicSendIpi(0, Vector, true, IncludeSelf);
}

// Frequency of the PIT and PMT timers in hertz.
#define FREQUENCY_PIT  (1193182)
#define FREQUENCY_PMT  (3579545)

#define NANOSECONDS_PER_SECOND (1000000000ULL)

//
// Calibrating timers!
//
// The principle is the same basically everywhere except the HPET.
// 1. Write 0xFFFFFFFF to the APIC, and the highest value to the anchor timer.
// 2. Wait a little bit, a for (1....100000 will do)
// 3. Read in the values, and do math on them to determine the frequency
//    of the APIC timer. (And the TSC too)
//

static void HalpCalculateTicks(uint64_t* ApicTicksOut, uint64_t* TscTicksOut, uint64_t TimerTicks, uint64_t ApicTicksIn, uint64_t TscTicksIn, uint64_t TimerFrequencyHz)
{
	uint64_t TimerNano = (uint64_t) TimerTicks * (NANOSECONDS_PER_SECOND / TimerFrequencyHz);
	
	// Scale the APIC and TSC ticks to be the number of ticks in a second.
	*ApicTicksOut = ApicTicksIn * NANOSECONDS_PER_SECOND / TimerNano;
	* TscTicksOut =  TscTicksIn * NANOSECONDS_PER_SECOND / TimerNano;
}

static void HalpResetApicCounter()
{
	// Set the initial APIC init counter to -1.
	ApicWriteRegister(APIC_REG_TMR_INIT_CNT, 0xFFFFFFFF);
}

static uint64_t HalpGetElapsedApicTicks()
{
	ApicWriteRegister(APIC_REG_LVT_TIMER, APIC_LVT_INT_MASKED);
	uint64_t ApicEnd = ApicReadRegister(APIC_REG_TMR_CURR_CNT);
	uint64_t ApicTicks = 0xFFFFFFFF - ApicEnd;
	return ApicTicks;
}

// Rounds a frequency value to the nearest multiple of 10^6.
static uint64_t HalpAttemptRoundFrequency(uint64_t Frequency)
{
	const uint64_t Multiple = 10000000;
	uint64_t T1 = Frequency / Multiple;
	
	// If T1 is zero, return the unmodified frequency since it's probably <10^6.
	if (!T1)
		return Frequency;
	
	T1 *= Multiple;
	
	if (T1 + Multiple / 2 < Frequency)
		T1 += Multiple;
	
	return T1;
}

// Prepare the timer for counting
typedef void(*TIMER_PREPARE_FUNC)();
// Read the value in the timer
typedef uint32_t(*TIMER_READ_FUNC)();
// Whether to continue spinning (i.e. if TickCount ticks have expired since TimerStart)
typedef bool(*TIMER_CONTINUE_SPINNING_FUNC)(uint32_t TimerStart, uint32_t TickCount);
// Get the number of ticks that have passed.
typedef int64_t(*TIMER_FINISH_FUNC)(uint64_t TimerStart, uint64_t TimerEnd);

static void HalpCalibrateApicGeneric(
	TIMER_PREPARE_FUNC TimerPrepare,
	TIMER_READ_FUNC TimerRead,
	TIMER_CONTINUE_SPINNING_FUNC TimerContinueSpinning,
	TIMER_FINISH_FUNC TimerFinish,
	uint32_t Runs,
	uint32_t TickCount,
	uint32_t Frequency
)
{
	uint64_t AverageApic = 0, AverageTsc = 0;
	
	for (uint32_t Run = 0; Run < Runs; Run++)
	{
		TimerPrepare();
		uint64_t TscStart   = HalReadTsc();
		uint64_t TimerStart = TimerRead();
		uint64_t TimerEnd;
		
		HalpResetApicCounter();
		
		while (TimerContinueSpinning(TimerStart, TickCount))
			KeSpinningHint();
		
		// Set!!
		if (TimerRead == HalReadPit)
			TimerEnd   = TimerRead();
		
		uint64_t ApicTicks = HalpGetElapsedApicTicks();
		uint64_t TscTicks  = HalReadTsc() - TscStart;
		
		if (TimerRead != HalReadPit)
			TimerEnd   = TimerRead();
		
		int64_t TimerTicks = TimerFinish(TimerStart, TimerEnd);
		
		HalpCalculateTicks(&ApicTicks,
		                   &TscTicks,
						   TimerTicks,
						   ApicTicks,
						   TscTicks,
						   Frequency);
		
		AverageApic += ApicTicks;
		AverageTsc  += TscTicks;
	}
	
	AverageApic /= Runs;
	AverageTsc  /= Runs;
	
	//DbgPrint("CPU %u reports APIC: %llu  TSC: %llu   Ticks/s",  KeGetCurrentPRCB()->LapicId,  AverageApic,  AverageTsc);
	
	KeGetCurrentHalCB()->LapicFrequency = HalpAttemptRoundFrequency(AverageApic);
	KeGetCurrentHalCB()->TscFrequency   = HalpAttemptRoundFrequency(AverageTsc);
	
	//DbgPrint("Rounded, we get %llu, respectively %llu Ticks/s", KeGetCurrentHalCB()->LapicFrequency, KeGetCurrentHalCB()->TscFrequency);
}

// ===== HPET =====

void HalCalibrateApicUsingHpet()
{
	// TODO
	//HalpCalibrateApicGeneric(HalHpetPrepare,
	//                         HalHpetRead,
	//                         HalHpetContinueSpinning,
	//                         HalHpetFinish,
	//                         16,
	//                         30000,
	//                         FREQUENCY_PMT);
}

// ===== ACPI Power Management Timer =====

void HalPmtPrepare()
{
}

bool HalPmtContinueSpinning(uint32_t TimerStart, uint32_t TickCount)
{
	int64_t Time = HalGetPmtCounter() - TimerStart;
	if (Time < 0)
		Time += 0x1000000ULL;
	
	return Time < TickCount;
}

int64_t HalPmtFinish(uint64_t TimerStart, uint64_t TimerEnd)
{
	int64_t Ticks = TimerEnd - TimerStart;
	
	// Mitigate overflow
	
	while (Ticks < 0)
		Ticks += 0x1000000ULL; // 1^24 since we pessimistically assume that the ACPI timer is 24 bit
	
	return Ticks;
}

void HalCalibrateApicUsingPmt()
{
	HalpCalibrateApicGeneric(HalPmtPrepare,
	                         HalGetPmtCounter,
	                         HalPmtContinueSpinning,
							 HalPmtFinish,
	                         16,
	                         30000,
	                         FREQUENCY_PMT);
}

// ===== PIT =====

void HalPitPrepare()
{
	HalWritePit(0xFFFF);
}

bool HalPitContinueSpinning(UNUSED uint32_t TimerStart, uint32_t TickCount)
{
	return HalReadPit() > 0xFFFF - TickCount;
}

int64_t HalPitFinish(uint64_t TimerStart, uint64_t TimerEnd)
{
	int64_t Ticks = TimerStart - TimerEnd;
	
	// Mitigate overflow
	while (Ticks < 0)
		Ticks += 0x10000ULL;
	
	return Ticks;
}

void HalCalibrateApicUsingPit()
{
	HalpCalibrateApicGeneric(HalPitPrepare,
	                         HalReadPit,
	                         HalPitContinueSpinning,
							 HalPitFinish,
	                         16,
	                         10000,
	                         FREQUENCY_PIT);
}

static KSPIN_LOCK HalpApicCalibLock;

// Calibrating the APIC
void HalCalibrateApic()
{
	KIPL OldIpl;
	KeAcquireSpinLock(&HalpApicCalibLock, &OldIpl);
	
	// Tell the APIC timer to use a divider of 2.
	// See the Intel SDM Vol.3A Ch.11 "11.5.4 APIC Timer". Note that bit 2 is reserved.
	ApicWriteRegister(APIC_REG_TMR_DIV_CFG, 0);
	
	if (false && HpetIsAvailable())
	{
		HalCalibrateApicUsingHpet();
		KeReleaseSpinLock(&HalpApicCalibLock, OldIpl);
		return;
	}
	
	if (HalAcpiIsPmtAvailable())
	{
		HalCalibrateApicUsingPmt();
		KeReleaseSpinLock(&HalpApicCalibLock, OldIpl);
		return;
	}
	
	HalCalibrateApicUsingPit();
	KeReleaseSpinLock(&HalpApicCalibLock, OldIpl);
	
	// if the TSC increases at more than 6 GHz. We have indeed reached 5 GHz
	if (KeGetCurrentHalCB()->TscFrequency >= 6000000000)
	{
		KeCrash("TSC frequency over 6 GHz?! Ain't no way!");
	}
}

void HalApicSetIrqIn(uint64_t Ticks)
{
	ApicWriteRegister(APIC_REG_LVT_TIMER,    HalpVectorApicTimer);
	ApicWriteRegister(APIC_REG_TMR_INIT_CNT, Ticks);
}

// Interrupt Handling

static PKREGISTERS HalpSpuriousHandler(PKREGISTERS Regs)
{
	ApicEndOfInterrupt();
	return Regs;
}

static PKREGISTERS HalpApicTimerHandler(PKREGISTERS Regs)
{
	KeTimerTick();
	ApicEndOfInterrupt();
	return Regs;
}

// Initialization

void HalInitApicUP()
{
	HalpVectorApicTimer = KeAllocateInterruptVector(IPL_CLOCK);
	HalpVectorSpurious  = KeAllocateInterruptVector(IPL_NOINTS);
	
	KeRegisterInterrupt(HalpVectorSpurious,  HalpSpuriousHandler);
	KeRegisterInterrupt(HalpVectorApicTimer, HalpApicTimerHandler);
	
	DbgPrint("HalpVectorApicTimer: %d", HalpVectorApicTimer);
}

void HalInitApicMP()
{
	// Set the spurious interrupt vector register bit 8 to start receiving interrupts
	ApicWriteRegister(APIC_REG_SPURIOUS, 0x100 | HalpVectorSpurious);
}


HAL_API void HalEndOfInterrupt()
{
	ApicEndOfInterrupt();
}

HAL_API void HalRequestIpi(uint32_t LapicId, uint32_t Flags, int Vector)
{
	ApicSendIpi(LapicId, Vector, Flags & HAL_IPI_BROADCAST, Flags & HAL_IPI_SELF);
}
