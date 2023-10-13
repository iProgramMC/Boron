// Boron64 - Interrupt handlers
#include <arch.h>
#include <except.h>
#include <ke.h>
#include <hal.h>
#include "../../hal/iapc/apic.h"

#define ENTER_INTERRUPT(ipl)              \
	KIPL __current_ipl = KeRaiseIPL(ipl); \
	KeSetInterruptsEnabled(true);

#define LEAVE_INTERRUPT                   \
	KeSetInterruptsEnabled(false);        \
	KeLowerIPL(__current_ipl);

static UNUSED void KiDumpCPURegs(KREGISTERS* Regs)
{
    LogMsg("cr2:    %llx", Regs->cr2);
    LogMsg("rip:    %llx", Regs->rip);
    LogMsg("rsp:    %llx", Regs->rsp);
    LogMsg("error:  %llx", Regs->error_code);
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
KREGISTERS* KiTrapUnknownHandler(KREGISTERS* Regs)
{
	ENTER_INTERRUPT(IPL_NOINTS);
	
	KeOnUnknownInterrupt(Regs->rip);
	
	LEAVE_INTERRUPT;
	return Regs;
}

// Double fault handler.
KREGISTERS* KiTrap08Handler(KREGISTERS* Regs)
{
	ENTER_INTERRUPT(IPL_NOINTS);
	
	KeOnDoubleFault(Regs->rip);
	
	LEAVE_INTERRUPT;
	return Regs;
}

// General protection fault handler.
KREGISTERS* KiTrap0DHandler(KREGISTERS* Regs)
{
	ENTER_INTERRUPT(IPL_NOINTS);
	
	KeOnProtectionFault(Regs->rip);
	
	LEAVE_INTERRUPT;
	return Regs;
}

// Page fault handler.
KREGISTERS* KiTrap0EHandler(KREGISTERS* Regs)
{
	ENTER_INTERRUPT(IPL_DPC);
	
	KeOnPageFault(Regs->rip, Regs->cr2, Regs->error_code);
	
	LEAVE_INTERRUPT;
	return Regs;
}

// DPC interrupt handler.
KREGISTERS* KeHandleDpcIpi(KREGISTERS*);
KREGISTERS* KiTrap40Handler(KREGISTERS* Regs)
{
	ENTER_INTERRUPT(IPL_DPC);
	
	KeHandleDpcIpi(Regs);
	HalApicEoi();
	
	LEAVE_INTERRUPT;
	return Regs;
}

// APIC timer interrupt.
void HalApicHandleInterrupt();

KREGISTERS* KiTrapF0Handler(KREGISTERS* Regs)
{
	HalApicHandleInterrupt();
	HalApicEoi();
	return Regs;
}

// TLB shootdown interrupt.
KREGISTERS* KiTrapFDHandler(KREGISTERS* Regs)
{
	KPRCB* myCPU = KeGetCurrentPRCB();
	
#ifdef DEBUG2
	SLogMsg("Handling TLB shootdown on CPU %u", myCPU->LapicId);
#endif
	
	for (size_t i = 0; i < myCPU->TlbsLength; i++)
	{
		KeInvalidatePage((void*)(myCPU->TlbsAddress + i * PAGE_SIZE));
	}
	
	KeReleaseSpinLock(&myCPU->TlbsLock, IPL_NOINTS);
	
	HalApicEoi();
	return Regs;
}

// Crash IPI (inter processor interrupt).
void HalpOnCrashedCPU();
KREGISTERS* KiTrapFEHandler(KREGISTERS* Regs)
{
	// We have received an interrupt from the processor that has crashed. We should crash too!
	
	// we have no plans to return from the interrupt, so ignore the return value of KeRaiseIPL.
	KeRaiseIPL(IPL_NOINTS);
	
	// notify the crash handler that we've stopped
	HalpOnCrashedCPU();
	
	// and stop!!
	KeStopCurrentCPU();
	
	return Regs;
}

// Spurious interrupt.
KREGISTERS* KiTrapFFHandler(KREGISTERS* Regs)
{
	HalApicEoi();
	return Regs;
}
