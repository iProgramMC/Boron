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

#include <hal.h>
#include <mm.h>
#include <ke.h>
#include "apic.h"
#include "acpi.h"
#include "hpet.h"
#include "pit.h"
#include "tsc.h"

#define SPURIOUS_INTERRUPT_VECTOR (0xFF)
#define APIC_TIMER_MODE_ONESHOT   (0b00 << 17)
#define APIC_TIMER_MODE_PERIODIC  (0b01 << 17) // not used right now, but may be needed.
#define APIC_TIMER_MODE_TSCDEADLN (0b10 << 17)

#define APIC_LVT_INT_MASKED (0x10000)

#define IA32_APIC_BASE_MSR (0x1B)

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

static void ApicSendIpi(uint32_t lapicID, int vector, bool broadcast, bool includeSelf)
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
		if (includeSelf)
			mode = APIC_ICR0_BROADCAST;
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

void HalApicEoi()
{
	ApicEndOfInterrupt();
}

void HalEnableApic()
{
	// Set the spurious interrupt vector register bit 8 to start receiving interrupts
	ApicWriteRegister(APIC_REG_SPURIOUS, 0x100 | INTV_SPURIOUS);
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

void HalCalibrateApicUsingHpet()
{
	// @TODO
	KeCrash("TODO: Calibrate using the HPET");
}

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

// @TODO: A lot of the math is the same. Deduplicate?
void HalCalibrateApicUsingPmt()
{
	// Runs, and count of pauses per run.
	const int Runs = 16, Count = 1000;
	
	uint64_t AverageApic = 0, AverageTsc = 0;
	
	for (int Run = 0; Run < Runs; Run++)
	{
		uint64_t TscStart = HalReadTsc();
		uint64_t PmtStart = HalGetPmtCounter();
		
		HalpResetApicCounter();
		
		// Sleep for an arbitrary number of iterations.
		int Counter = Count;
		while (Counter--)
			KeSpinningHint();
		
		// Stop the clock!
		uint64_t PmtEnd    = HalGetPmtCounter();
		uint64_t ApicTicks = HalpGetElapsedApicTicks();
		uint64_t TscTicks  = HalReadTsc() - TscStart;
		
		int64_t PmtTicks = PmtEnd - PmtStart + 3; // 3 - balancing
		
		// Mitigate Overflow
		while (PmtTicks < 0)
			PmtTicks += 0x100000000ULL;
		
		HalpCalculateTicks(&ApicTicks,
		                   &TscTicks,
						   PmtTicks,
						   ApicTicks,
						   TscTicks,
						   FREQUENCY_PMT);
		
		AverageApic += ApicTicks;
		AverageTsc  += TscTicks;
	}
	
	AverageApic /= Runs;
	AverageTsc  /= Runs;
	
	LogMsg("CPU %u reports APIC: %llu  TSC: %llu   Ticks/s",  KeGetCurrentPRCB()->LapicId,  AverageApic,  AverageTsc);
}

void HalCalibrateApicUsingPit()
{
	// Runs, and count of pauses per run.
	const int Runs = 16, Count = 1000;
	
	uint64_t AverageApic = 0, AverageTsc = 0;
	
	for (int Run = 0; Run < Runs; Run++)
	{
		uint64_t TscStart = HalReadTsc();
		
		// Prepare the PIT for the countdown
		HalWritePit(0xFFFF);
		
		HalpResetApicCounter();
		
		// Sleep for an arbitrary number of iterations.
		int Counter = Count;
		while (Counter--)
			KeSpinningHint();
		
		ApicWriteRegister(APIC_REG_LVT_TIMER, APIC_LVT_INT_MASKED);
		
		// Stop the clock!
		uint16_t PitEnd    = HalReadPit();
		uint64_t ApicTicks = HalpGetElapsedApicTicks();
		uint64_t TscTicks  = HalReadTsc() - TscStart;
		
		int PitTicks = 0xFFFF - PitEnd;
		
		// Mitigate Overflow
		while (PitTicks < 0)
			PitTicks += 0x10000;
		
		HalpCalculateTicks(&ApicTicks,
		                   &TscTicks,
						   PitTicks,
						   ApicTicks,
						   TscTicks,
						   FREQUENCY_PIT);
		
		AverageApic += ApicTicks;
		AverageTsc  += TscTicks;
	}
	
	AverageApic /= Runs;
	AverageTsc  /= Runs;
	
	LogMsg("CPU %u reports APIC: %llu  TSC: %llu   Ticks/s",  KeGetCurrentPRCB()->LapicId,  AverageApic,  AverageTsc);
}

static KSPIN_LOCK HalpApicCalibLock;

// Calibrating the APIC
void HalCalibrateApic()
{
	KeAcquireSpinLock(&HalpApicCalibLock);
	
	// Tell the APIC timer to use a divider of 2.
	// See the Intel SDM Vol.3A Ch.11 "11.5.4 APIC Timer". Note that bit 2 is reserved.
	ApicWriteRegister(APIC_REG_TMR_DIV_CFG, 0);
	
	if (false && HpetIsAvailable())
	{
		HalCalibrateApicUsingHpet();
		KeReleaseSpinLock(&HalpApicCalibLock);
		return;
	}
	
	if (HalAcpiIsPmtAvailable())
	{
		HalCalibrateApicUsingPmt();
		KeReleaseSpinLock(&HalpApicCalibLock);
		return;
	}
	
	
	HalCalibrateApicUsingPit();
	KeReleaseSpinLock(&HalpApicCalibLock);
}
