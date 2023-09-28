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

#define C_SPURIOUS_INTERRUPT_VECTOR (0xFF)
#define C_APIC_TIMER_DIVIDE_BY_128  (0b1010) // Intel SDM Vol.3A Ch.11 "11.5.4 APIC Timer". Bit 2 is reserved.
#define C_APIC_TIMER_DIVIDE_BY_16   (0b0011) // Intel SDM Vol.3A Ch.11 "11.5.4 APIC Timer". Bit 2 is reserved.
#define C_APIC_TIMER_MODE_ONESHOT   (0b00 << 17)
#define C_APIC_TIMER_MODE_PERIODIC  (0b01 << 17) // not used right now, but may be needed.
#define C_APIC_TIMER_MODE_TSCDEADLN (0b10 << 17)

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

static bool ApicCheckExists()
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
