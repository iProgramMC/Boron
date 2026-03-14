/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/i386/traps.c
	
Abstract:
	This header file implements support for the IDT (Interrupt
	Dispatch Table).
	
Author:
	iProgramInCpp - 28 October 2025
***/
#include <arch.h>
#include <string.h>
#include <ke.h>
#include <hal.h>
#include <except.h>
#include "../../ke/ki.h"

extern void* const KiTrapList[];     // traplist.asm
extern int8_t      KiTrapIplList[];  // trap.asm
extern void*       KiTrapCallList[]; // trap.asm

extern void KiCallSoftwareInterrupt(int Vector); // trap.asm

// The trap gate isn't likely to be used as it doesn't turn off
// interrupts when entering the interrupt handler.
enum KGATE_TYPE
{
	GATE_INT  = 0xE,
	GATE_TRAP = 0xF
};

// The IDT itself.
KIDT KiIdt;

struct KIDT_DESCRIPTOR_tag
{
	uint16_t Limit;
	PKIDT    Base;
}
PACKED
KiIdtDescriptor;

// The interrupt vector function type.  It's not a function you can actually call from C.
typedef void(*KiInterruptVector)();

static UNUSED void KiSetInterruptDPL(PKIDT Idt, int Vector, int Ring)
{
	Idt->Entries[Vector].DPL = Ring;
}

// This is also not likely to be used. We need interrupts to be disabled automatically
// before we raise the IPL to the interrupt's level, but trap gates will not disable interrupts.
// This is bad, since a keyboard interrupt could manage to sneak past an important clock interrupt
// before we manage to raise the IPL in the clock interrupt.
// Not to mention the performance gains would be minimal if at all existent.
static UNUSED void KiSetInterruptGateType(PKIDT Idt, int Vector, int GateType)
{
	Idt->Entries[Vector].GateType = GateType;
}

// Set the IST of an interrupt vector. This is probably useful for the double fault handler,
// as it is triggered only when another interrupt failed to be called, which is useful in cases
// where the kernel stack went missing and bad (Which I hope there aren't any!)
static UNUSED void KiSetInterruptStackIndex(PKIDT Idt, int Vector, int Ist)
{
	Idt->Entries[Vector].IST = Ist;
}

// This loads the interrupt vector handler into the IDT.
// Parameters:
// Idt     - The IDT that the interrupt vector will be loaded in.
// Vector  - The interrupt number that the handler will be assigned to.
static void KiLoadInterruptVector(PKIDT Idt, int Vector, KiInterruptVector Handler)
{
	PKIDT_ENTRY Entry = &Idt->Entries[Vector];
	memset(Entry, 0, sizeof * Entry);
	
	uintptr_t HandlerAddr = (uintptr_t) Handler;
	
	Entry->OffsetLow  = HandlerAddr & 0xFFFF;
	Entry->OffsetHigh = HandlerAddr >> 16;
	
	// The code segment that the interrupt handler will run in.
	Entry->SegmentSel = SEG_RING_0_CODE;
	
	// Ist = 0 means the stack is not switched when the interrupt keeps the CPL the same.
	// The IST should only be used in case of a fatal exception, such as a double fault.
	Entry->IST = 0;
	
	// The ring that the interrupt can be called from. It's usually 0 (so you can't fake
	// an exception from user mode), however, it can be set to 3 for userspace system calls.
	// Usually we use `syscall` or `sysenter` for that, though.
	Entry->DPL = 0;
	
	// This is an interrupt gate.
	Entry->GateType = 0xE;
	
	// The entry is present.
	Entry->Present = true;
}

int KiTryEnterHardwareInterrupt(int IntNum)
{
	PKPRCB Prcb = KeGetCurrentPRCB();
	
	PKIPL IplPtr = &Prcb->Ipl;
	
	// grab old IPL
	int OldIpl = (int) *IplPtr;
	
	// figure out the new IPL
	int NewIpl = KiTrapIplList[IntNum];
	
	// Check if this is a lower priority interrupt than our current IPL.
	if (NewIpl != -1)
	{
		// If this is a hardware interrupt and it's lower in our priority, then defer it.
		if (OldIpl >= NewIpl && IntNum >= 0x20)
		{
			DbgPrint("Deferring int %d for later.  TriedIpl: %d  CurrIpl: %d", IntNum, NewIpl, OldIpl);
			
			if (IntNum == PIC_INTERRUPT_BASE)
				KeCrash("Timer tick triggered twice??");
		
			// Trying to get interrupted by a masked interrupt? No problem, enqueue
			// this interrupt onto our queue and simply return.
			if (Prcb->ArchData.InterruptQueuePlace >= KE_MAX_QUEUED_INTERRUPTS)
			{
				DbgPrint(
					"ERROR: Got interrupt %d. More than %d interrupts enqueued "
					"at once, so this interrupt will be dropped.",
					KE_MAX_QUEUED_INTERRUPTS
				);
				return -1;
			}
			
			Prcb->ArchData.InterruptQueue[Prcb->ArchData.InterruptQueuePlace++] = IntNum;
			return -1;
		}
		
		*IplPtr = NewIpl; // specific to Amd64
	}
	
	// now that we've setup the hardware interrupt stuff, enable interrupts.
	// we couldn't have done that before because the CPU would think that we're
	// in a low IPL thing meanwhile we're not..
	if (NewIpl != -1 && NewIpl < IPL_CLOCK)
		ENABLE_INTERRUPTS();
	
	return OldIpl;
}

void KiExitHardwareInterrupt(PKREGISTERS Registers)
{
	DISABLE_INTERRUPTS();
	PKPRCB Prcb = KeGetCurrentPRCB();
	int OldIpl = Registers->OldIpl;
	Prcb->Ipl = OldIpl;
	
	// Check if there are any interrupts we still need to de-queue.
	// Note that we don't scan using a for loop, because this could change.
	while (Prcb->ArchData.InterruptQueuePlace != 0)
	{
		int Vector = Prcb->ArchData.InterruptQueue[--Prcb->ArchData.InterruptQueuePlace];
		DbgPrint("Calling enqueued software interrupt %d", Vector);
		KiCallSoftwareInterrupt(Vector);
	}
	
	// Check if the current thread is terminated and we are about to
	// return to user mode.
	PKTHREAD CurrentThread = KeGetCurrentThread();
	if (CurrentThread &&
		CurrentThread->PendingTermination &&
		Registers->OldIpl == IPL_NORMAL &&
		Registers->Cs == SEG_RING_3_CODE)
	{
		KiTerminateUserModeThread(CurrentThread->IncrementTerminated);
	}
	
	// Note: safe to call here because KiDispatchSoftwareInterrupts
	// preserves interrupt disable state across a call to it
	if (Prcb->PendingSoftInterrupts >> OldIpl)
		KiDispatchSoftwareInterrupts(OldIpl);
}

void KiCheckTerminatedUserMode()
{
	// Before entering user mode, check if the thread was terminated first.
	if (KeGetCurrentThread()->PendingTermination)
		KiTerminateUserModeThread(KeGetCurrentThread()->IncrementTerminated);
}

// ==== Interrupt Handlers ====

// Generic interrupt handler. Used in case an interrupt is not implemented
PKREGISTERS KiTrapUnknownHandler(PKREGISTERS Regs)
{
	KeOnUnknownInterrupt(Regs);
	return Regs;
}

PKREGISTERS KiHandleDoubleFault(PKREGISTERS Regs)
{
	KeOnDoubleFault(Regs);
	return Regs;
}

PKREGISTERS KiHandleProtectionFault(PKREGISTERS Regs)
{
	KeOnProtectionFault(Regs);
	return Regs;
}

PKREGISTERS KiHandleUndefinedInstructionFault(PKREGISTERS Regs)
{
	KeOnUndefinedInstruction(Regs);
	return Regs;
}

PKREGISTERS KiHandlePageFault(PKREGISTERS Regs)
{
	KeOnPageFault(Regs);
	return Regs;
}

static KSPIN_LOCK KiTrapLock;

extern PKREGISTERS KiSystemServiceHandler(PKREGISTERS Regs);

// Run on the BSP only.
void KiSetupIdt()
{
	KiIdtDescriptor.Base  = &KiIdt;
	KiIdtDescriptor.Limit = sizeof(KiIdt) - 1;
	
	for (int i = 0; i < 0x100; i++)
	{
		KiLoadInterruptVector(&KiIdt, i, KiTrapList[i]);
		
		KiTrapIplList[i]  = IPL_NOINTS;
		KiTrapCallList[i] = KiTrapUnknownHandler;
	}
	
	KeRegisterInterrupt(INTV_DBL_FAULT,  KiHandleDoubleFault);
	KeRegisterInterrupt(INTV_PROT_FAULT, KiHandleProtectionFault);
	KeRegisterInterrupt(INTV_PAGE_FAULT, KiHandlePageFault);
	KeRegisterInterrupt(INTV_SYSTEMCALL, KiSystemServiceHandler);
	KeSetInterruptIPL(INTV_DBL_FAULT,  IPL_NOINTS);
	KeSetInterruptIPL(INTV_PROT_FAULT, IPL_NOINTS);
	KeSetInterruptIPL(INTV_PAGE_FAULT, IPL_UNDEFINED);
	KeSetInterruptIPL(INTV_SYSTEMCALL, IPL_UNDEFINED);
	KiSetInterruptDPL(&KiIdt, INTV_SYSTEMCALL, 3);
}

void KeRegisterInterrupt(int Vector, PKINTERRUPT_HANDLER Handler)
{
	KIPL Ipl;
	KeAcquireSpinLock(&KiTrapLock, &Ipl);
	KiTrapCallList[Vector] = Handler;
	KeReleaseSpinLock(&KiTrapLock, Ipl);
}

void KeSetInterruptIPL(int Vector, KIPL Ipl)
{
	KiTrapIplList[Vector] = Ipl;
}

void KeOnUpdateIPL(UNUSED KIPL OldIpl, UNUSED KIPL NewIpl)
{
}
