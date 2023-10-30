// Boron64 - Interrupt management
#include <arch.h>
#include <string.h>
#include <ke.h>
#include <hal.h>
#include <except.h>
#include "../../ke/ki.h"

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

// Probably not going to be used because SYSCALL and SYSENTER both bypass the interrupt system.
// These are going to be optimized out, and are also niceties when needed, so I will keep them.
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

int KiEnterHardwareInterrupt(int NewIpl)
{
	PKIPL IplPtr = &KeGetCurrentPRCB()->Ipl;
	
	// grab old IPL
	int OldIpl = (int) *IplPtr;
	
	// set new IPL, if not marked as "don't override"
	if (NewIpl != -1)
	{
		*IplPtr = NewIpl; // specific to Amd64
		
		if (OldIpl > NewIpl)
			// uh oh!
			KeCrash("KiEnterHardwareInterrupt: Old IPL of %d was higher than current IPL of %d.", OldIpl, NewIpl);
		
		KeOnUpdateIPL(NewIpl, OldIpl);
	}
	
	// now that we've setup the hardware interrupt stuff, enable interrupts.
	// we couldn't have done that before because the CPU would think that we're
	// in a low IPL thing meanwhile we're not..
	ENABLE_INTERRUPTS();
	
	return OldIpl;
}

void KiExitHardwareInterrupt(int OldIpl)
{
	DISABLE_INTERRUPTS();
	
	if (OldIpl == MAGIC_IPL)
		OldIpl = IPL_DPC;
	
	PKIPL IplPtr = &KeGetCurrentPRCB()->Ipl;
	KIPL PrevIpl = *IplPtr;
	*IplPtr = OldIpl;
	KeOnUpdateIPL(OldIpl, PrevIpl);
}

// ==== Interrupt Handlers ====

// Generic interrupt handler. Used in case an interrupt is not implemented
PKREGISTERS KiTrapUnknownHandler(PKREGISTERS Regs)
{
	KeOnUnknownInterrupt(Regs->rip, Regs->IntNumber);
	return Regs;
}

PKREGISTERS KiHandleDpcIpi(PKREGISTERS Regs)
{
	PKREGISTERS New = KiHandleSoftIpi(Regs);
	HalEndOfInterrupt();
	return New;
}

PKREGISTERS KiHandleDoubleFault(PKREGISTERS Regs)
{
	KeOnDoubleFault(Regs->rip);
	return Regs;
}

PKREGISTERS KiHandleProtectionFault(PKREGISTERS Regs)
{
	KeOnProtectionFault(Regs->rip);
	return Regs;
}

PKREGISTERS KiHandlePageFault(PKREGISTERS Regs)
{
	KeOnPageFault(Regs->rip, Regs->cr2, Regs->ErrorCode);
	return Regs;
}

extern void* const KiTrapList[];     // traplist.asm
extern int8_t      KiTrapIplList[];  // trap.asm
extern void*       KiTrapCallList[]; // trap.asm

static int KepIplVectors[IPL_COUNT];

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
	
	for (int i = 0; i < IPL_COUNT; i++)
	{
		KepIplVectors[i] = i * 0x10;
	}
	
	KeRegisterInterrupt(INTV_DBL_FAULT,  KiHandleDoubleFault);
	KeRegisterInterrupt(INTV_PROT_FAULT, KiHandleProtectionFault);
	KeRegisterInterrupt(INTV_PAGE_FAULT, KiHandlePageFault);
	KeSetInterruptIPL(INTV_DBL_FAULT,  IPL_NOINTS);
	KeSetInterruptIPL(INTV_PROT_FAULT, IPL_NOINTS);
	KeSetInterruptIPL(INTV_PAGE_FAULT, IPL_UNDEFINED);
}

// NOTE: This works fine on AMD64 because we can reprogram the IOAPIC
// to give us whichever interrupt number we want.

int KeAllocateInterruptVector(KIPL Ipl)
{
	if (Ipl < 0 || Ipl > IPL_NOINTS)
		return -1;
	
	if (KepIplVectors[Ipl] >= (Ipl + 1) * 0x10)
		// We return -1 instead of crashing so that device drivers
		// can opt for different IPLs in case one doesn't work
		return -1;
	
	int Vector = KepIplVectors[Ipl]++;
	
	// Set the IPL that we expect to run at
	KiTrapIplList[Vector] = Ipl;
	return Vector;
}

void KeRegisterInterrupt(int Vector, PKINTERRUPT_HANDLER Handler)
{
	KiTrapCallList[Vector] = Handler;
}

void KeSetInterruptIPL(int Vector, KIPL Ipl)
{
	KiTrapIplList[Vector] = Ipl;
}
