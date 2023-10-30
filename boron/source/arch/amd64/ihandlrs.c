// Boron64 - Interrupt handlers
#include <arch.h>
void KiDumpCPURegs(PKREGISTERS Regs)
{
	DbgPrint("OldIpl=%p",Regs->OldIpl);
	DbgPrint("DS=%02x,ES=%02x,FS=%02x,GS=%02x",Regs->ds,Regs->es,Regs->fs,Regs->gs);
	DbgPrint("CR2=%p,RBP=%p,RDI=%p,RSI=%p",Regs->cr2,Regs->rbp,Regs->rdi,Regs->rsi);
	DbgPrint("R15=%p,R14=%p,R13=%p,R12=%p",Regs->r15,Regs->r14,Regs->r13,Regs->r12);
	DbgPrint("R11=%p,R10=%p,R9 =%p,R8 =%p",Regs->r11,Regs->r10,Regs->r9, Regs->r8);
	DbgPrint("RAX=%p,RBX=%p,RCX=%p,RDX=%p",Regs->rax,Regs->rbx,Regs->rcx,Regs->rdx);
	DbgPrint("IntNum=%p  ErrCode=%p",Regs->IntNumber, Regs->ErrorCode);
	DbgPrint("RIP=%p,CS=%02x",Regs->rip,Regs->cs);
	DbgPrint("RFLAGS=%p",Regs->rflags);
	DbgPrint("RSP=%p,SS=%02x",Regs->rsp,Regs->ss);
}

#if 0

#include "../../ke/ki.h"
#include <arch.h>
#include <except.h>
#include <hal.h>
#include "../../hal/iapc/apic.h"
#include "../../hal/iapc/ps2.h"

static UNUSED void KiDumpCPURegs(PKREGISTERS Regs)
{
    LogMsg("cr2:    %llx", Regs->cr2);
    LogMsg("rip:    %llx", Regs->rip);
    LogMsg("rsp:    %llx", Regs->rsp);
    LogMsg("error:  %llx", Regs->ErrorCode);
    LogMsg("vect:   %02x", Regs->IntNumber);
    LogMsg("cs:     %llx", Regs->cs);
    LogMsg("ss:     %llx", Regs->ss);
    LogMsg("ds:     %llx", Regs->ds);
    LogMsg("es:     %llx", Regs->es);
    LogMsg("fs:     %llx", Regs->fs);
    LogMsg("gs:     %llx", Regs->gs);
    LogMsg("rflags: %llx", Regs->rflags);
    LogMsg("rax:    %llx", Regs->rax);
    LogMsg("rbx:    %llx", Regs->rbx);
    LogMsg("rcx:    %llx", Regs->rcx);
    LogMsg("rdx:    %llx", Regs->rdx);
    LogMsg("rsi:    %llx", Regs->rsi);
    LogMsg("rdi:    %llx", Regs->rdi);
    LogMsg("rbp:    %llx", Regs->rbp);
    LogMsg("r8:     %llx", Regs->r8);
    LogMsg("r9:     %llx", Regs->r9);
    LogMsg("r10:    %llx", Regs->r10);
    LogMsg("r11:    %llx", Regs->r11);
    LogMsg("r12:    %llx", Regs->r12);
    LogMsg("r13:    %llx", Regs->r13);
    LogMsg("r14:    %llx", Regs->r14);
    LogMsg("r15:    %llx", Regs->r15);
}

// Generic interrupt handler.
// Used in case an interrupt is not implemented
PKREGISTERS KiTrapUnknownHandler(PKREGISTERS Regs)
{
	KeOnUnknownInterrupt(Regs->rip);
	return Regs;
}

// Double fault handler.
PKREGISTERS KiTrap08Handler(PKREGISTERS Regs)
{
	KeOnDoubleFault(Regs->rip);
	return Regs;
}

// General protection fault handler.
PKREGISTERS KiTrap0DHandler(PKREGISTERS Regs)
{
	KeOnProtectionFault(Regs->rip);
	return Regs;
}

// Page fault handler.
PKREGISTERS KiTrap0EHandler(PKREGISTERS Regs)
{
	KeOnPageFault(Regs->rip, Regs->cr2, Regs->error_code);
	return Regs;
}

// DPC interrupt handler.
PKREGISTERS KiTrap40Handler(PKREGISTERS Regs)
{
	PKREGISTERS New = KiHandleSoftIpi(Regs);
	HalEndOfInterrupt();
	return New;
}

// Keyboard interrupt handler.
PKREGISTERS KiTrap50Handler(PKREGISTERS Regs)
{
	HalHandleKeyboardInterrupt();
	HalEndOfInterrupt();
	return Regs;
}

// APIC timer interrupt.
void HalApicHandleInterrupt();

PKREGISTERS KiTrapF0Handler(PKREGISTERS Regs)
{
	HalApicHandleInterrupt();
	HalEndOfInterrupt();
	return Regs;
}

// TLB shootdown interrupt.
PKREGISTERS KiTrapFDHandler(PKREGISTERS Regs)
{
	KPRCB* myCPU = KeGetCurrentPRCB();
	
#ifdef DEBUG2
	DbgPrint("Handling TLB shootdown on CPU %u", myCPU->LapicId);
#endif
	
	for (size_t i = 0; i < myCPU->TlbsLength; i++)
	{
		KeInvalidatePage((void*)(myCPU->TlbsAddress + i * PAGE_SIZE));
	}
	
	KeReleaseSpinLock(&myCPU->TlbsLock, IPL_NOINTS);
	
	HalEndOfInterrupt();
	return Regs;
}

// Crash IPI (inter processor interrupt).
PKREGISTERS KiTrapFEHandler(UNUSED PKREGISTERS Regs)
{
	KeDisableInterrupts();
	
	// We have received an interrupt from the processor that has crashed. We should crash too!
	// Interrupts are disabled at this point
	
	// Notify the HAL that we've crashed. This function doesn't return;
	HalProcessorCrashed();
}

// Spurious interrupt.
PKREGISTERS KiTrapFFHandler(PKREGISTERS Regs)
{
	HalEndOfInterrupt();
	return Regs;
}

#endif

