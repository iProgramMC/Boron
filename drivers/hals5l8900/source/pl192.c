/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ha/pl192.c
	
Abstract:
	This module contains the implementation of the PL192
	VIC (vectored interrupt controller) driver.
	
Author:
	iProgramInCpp - 16 March 2026
***/
#include "hali.h"
#include <mm.h>
#include <string.h>

// N.B.  Each PL192 has support for 32 interrupts, so 64 total
#define MAX_INTERRUPTS 64

#define VIC0_BASE_PHYS (0x38E00000)
#define VIC1_BASE_PHYS (0x38E01000)

#define VICREG(n, rgof) (* (volatile uint32_t*) ((uintptr_t)HalVic ## n ## Base + rgof))

#define VICIRQSTATUS(n)      VICREG(n, 0x000)
#define VICFIQSTATUS(n)      VICREG(n, 0x004)
#define VICRAWINTR(n)        VICREG(n, 0x008)
#define VICINTSELECT(n)      VICREG(n, 0x00C)
#define VICINTENABLE(n)      VICREG(n, 0x010)
#define VICINTENCLEAR(n)     VICREG(n, 0x014)
#define VICSOFTINT(n)        VICREG(n, 0x018)
#define VICSOFTINTCLEAR(n)   VICREG(n, 0x01C)
#define VICPROTECTION(n)     VICREG(n, 0x020)
#define VICSWPRIORITYMASK(n) VICREG(n, 0x024)
#define VICPRIORITYDAISY(n)  VICREG(n, 0x028)
#define VICVECTADDR(n)     (&VICREG(n, 0x100)) // index as VICVECTADDR(VIC#)[INT#].  INT# in [0,31]
#define VICVECTPRIORITY(n) (&VICREG(n, 0x200)) // index as VICVECTPRIORITY(VIC#)[INT#].  INT# in [0,31]
#define VICADDRESS(n)        VICREG(n, 0xF00)  // weird place but OK

static void* HalVic0Base;
static void* HalVic1Base;

static KIPL HalInterruptIpls[MAX_INTERRUPTS];

void HalInitPL192()
{
	// step 1: map the VICs
	HalVic0Base = MmMapIoSpace(
		VIC0_BASE_PHYS,
		PAGE_SIZE,
		MM_PROT_READ | MM_PROT_WRITE | MM_MISC_DISABLE_CACHE,
		POOL_TAG("Vic0")
	);
	
	HalVic1Base = MmMapIoSpace(
		VIC1_BASE_PHYS,
		PAGE_SIZE,
		MM_PROT_READ | MM_PROT_WRITE | MM_MISC_DISABLE_CACHE,
		POOL_TAG("Vic1")
	);
	
	if (!HalVic0Base || !HalVic1Base)
		KeCrash("Could not map pl192 VICs");
	
	// N.B. OpeniBoot appears to write to something called an edgeic?
	// I'll configure it later, if it turns out that my kernel will not work
	// on real hardware.
	
	// mask all interrupts
	VICINTENCLEAR(0) = 0xFFFFFFFF;
	VICINTENCLEAR(1) = 0xFFFFFFFF;
	
	// 0 means use IRQs, 1 means use FIQs
	VICINTSELECT(0) = 0;
	VICINTSELECT(1) = 0;
	
	// Unmask all 16 interrupt levels
	VICSWPRIORITYMASK(0) = 0xFFFF;
	VICSWPRIORITYMASK(1) = 0xFFFF;
	
	// Set interrupt vector addresses to the interrupt number.
	for (int i = 0; i < 0x20; i++)
	{
		VICVECTADDR(0)[i] = i;
		VICVECTADDR(1)[i] = 0x20 + i;
		
		VICVECTPRIORITY(0)[i] = 0xF;
		VICVECTPRIORITY(1)[i] = 0xF;
	}
	
	// Clear the IPL list.
	for (int i = 0; i < MAX_INTERRUPTS; i++)
		HalInterruptIpls[i] = IPL_UNDEFINED;
	
	// Acknowledge all possible interrupts.
	for (int i = 0; i < 0x20; i++)
	{
		VICADDRESS(0) = 0;
		VICADDRESS(1) = 0;
	}
}

int HalGetMaximumInterruptCount()
{
	return MAX_INTERRUPTS;
}

void HalVicRegisterInterrupt(int InterruptNumber, KIPL Ipl)
{
	HalInterruptIpls[InterruptNumber] = Ipl;
	
	// Also let the hardware know.
	//
	// NOTE: Ipl is 4-bit, and it's also 4-bit in arch/arm/ipl.h, so we're good.
	// However, we do have to invert the priorities, since 0xF is highest and 0 is lowest.
	int Priority = 0xF - Ipl;
	if (InterruptNumber >= 32)
	{
		VICVECTPRIORITY(1)[InterruptNumber & 0x1F] = Priority;
		VICINTENABLE(1) = 1U << (InterruptNumber & 0x1F);
	}
	else
	{
		VICVECTPRIORITY(0)[InterruptNumber & 0x1F] = Priority;
		VICINTENABLE(0) = 1U << (InterruptNumber & 0x1F);
	}
}

void HalVicDeregisterInterrupt(int InterruptNumber, UNUSED KIPL Ipl)
{
	HalInterruptIpls[InterruptNumber] = IPL_UNDEFINED;
	
	if (InterruptNumber >= 32)
		VICINTENCLEAR(1) = 1U << (InterruptNumber & 0x1F);
	else
		VICINTENCLEAR(0) = 1U << (InterruptNumber & 0x1F);
}

void HalOnUpdateIpl(KIPL NewIpl, UNUSED KIPL OldIpl)
{
	if (!HalVic0Base || !HalVic1Base)
		return;
	
	// 0 is lowest, 1 is highest, meaning that the mask goes 0b111...000
	int Mask = 0xFFFF;
	if (NewIpl != IPL_NORMAL)
		Mask = (0xFFFF >> (NewIpl + 1)) & 0xFFFF;
	
	VICSWPRIORITYMASK(0) = Mask;
	VICSWPRIORITYMASK(1) = Mask;
}

PKREGISTERS HalOnFastInterruptRequest(PKREGISTERS Registers)
{
	DbgPrint("%s NYI", __func__);
	return Registers;
}

void HalRequestIpi(uint32_t LapicId, uint32_t Flags, int Vector)
{
	(void) LapicId;
	(void) Flags;
	(void) Vector;
	DbgPrint("%s NYI", __func__);
}

void HalEndOfInterrupt(int InterruptNumber)
{
	if (InterruptNumber >= 32) {
		VICADDRESS(1) = 0;
	}
	else {
		VICADDRESS(0) = 0;
	}
}

KIPL HalEnterHardwareInterrupt(int InterruptNumber)
{
	PKPRCB Prcb = KeGetCurrentPRCB();
	ASSERT(Prcb);
	
	PKIPL IplPtr = &Prcb->Ipl;
	KIPL OldIpl = (int) *IplPtr;
	KIPL NewIpl = HalInterruptIpls[InterruptNumber];
	
	if (NewIpl != IPL_UNDEFINED)
	{
		*IplPtr = NewIpl;
		
		if (OldIpl > NewIpl)
			// uh oh!
			KeCrash("HalEnterHardwareInterrupt: Old IPL of %d was higher than current IPL of %d.", OldIpl, NewIpl);
		
		HalOnUpdateIpl(NewIpl, OldIpl);
	}
	
	// now that we've set up the hardware interrupt stuff, enable interrupts.
	ENABLE_INTERRUPTS();
	return OldIpl;
}

void HalExitHardwareInterrupt(int InterruptNumber, KIPL OldIpl)
{
	DISABLE_INTERRUPTS();
	
	PKPRCB Prcb = KeGetCurrentPRCB();
	ASSERT(Prcb);
	
	KIPL PrevIpl = Prcb->Ipl;
	Prcb->Ipl = OldIpl;
	HalOnUpdateIpl(OldIpl, PrevIpl);
	
	(void) InterruptNumber;
	
	// Note: safe to call here because KeDispatchPendingSoftInterrupts
	// preserves interrupt disable state across a call to it
	KeDispatchPendingSoftInterrupts();
}

PKREGISTERS HalOnInterruptRequest(PKREGISTERS Registers)
{
	// Interrupts are disabled at this point.  Also, the VIC should handle IPLs for us.
	int InterruptNumber = -1;
	
	// TODO: this might be bad if a higher priority interrupt from VIC1 is taken.
	// Thankfully the timer is on IRQ 7, so it should be okay.
	if (VICIRQSTATUS(0) != 0)
	{
		InterruptNumber = (int) VICADDRESS(0);
	}
	else if (VICIRQSTATUS(1) != 0)
	{
		InterruptNumber = 32 + (int) VICADDRESS(1);
	}
	else
	{
		DbgPrintString("HalOnInterruptRequest: spurious interrupt?\n");
		return Registers;
	}
	
	KIPL OldIpl = HalEnterHardwareInterrupt(InterruptNumber);
	KeDispatchInterruptRequest(InterruptNumber);
	HalExitHardwareInterrupt(InterruptNumber, OldIpl);
	return Registers;
}
