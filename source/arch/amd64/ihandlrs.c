// Boron64 - Interrupt handlers
#include <arch.h>
#include <except.h>
#include <ke.h>
#include <hal.h>
#include "../../ha/apic.h"

#define ENTER_INTERRUPT(ipl)              \
	eIPL __current_ipl = KeRaiseIPL(ipl); \
	KeSetInterruptsEnabled(true);

#define LEAVE_INTERRUPT                   \
	KeSetInterruptsEnabled(false);        \
	KeLowerIPL(__current_ipl);

static UNUSED void KiDumpCPUState(CPUState* State)
{
    LogMsg("cr2:    %llx",State->cr2);
    LogMsg("rip:    %llx",State->rip);
    LogMsg("rsp:    %llx",State->rsp);
    LogMsg("error:  %llx",State->error_code);
    LogMsg("cs:     %llx",State->cs);
    LogMsg("ss:     %llx",State->ss);
    LogMsg("ds:     %llx",State->ds);
    LogMsg("es:     %llx",State->es);
    LogMsg("fs:     %llx",State->fs);
    LogMsg("gs:     %llx",State->gs);
    LogMsg("rflags: %llx",State->rflags);
    LogMsg("rax:    %llx",State->rax);
    LogMsg("rbx:    %llx",State->rbx);
    LogMsg("rcx:    %llx",State->rcx);
    LogMsg("rdx:    %llx",State->rdx);
    LogMsg("rsi:    %llx",State->rsi);
    LogMsg("rdi:    %llx",State->rdi);
    LogMsg("rbp:    %llx",State->rbp);
    LogMsg("r8:     %llx",State->r8);
    LogMsg("r9:     %llx",State->r9);
    LogMsg("r10:    %llx",State->r10);
    LogMsg("r11:    %llx",State->r11);
    LogMsg("r12:    %llx",State->r12);
    LogMsg("r13:    %llx",State->r13);
    LogMsg("r14:    %llx",State->r14);
    LogMsg("r15:    %llx",State->r15);
}

// Generic interrupt handler.
// Used in case an interrupt is not implemented
void KiTrapUnknownHandler(CPUState* State)
{
	ENTER_INTERRUPT(IPL_NOINTS);
	
	KeOnUnknownInterrupt(State->rip);
	
	LEAVE_INTERRUPT;
}

// Double fault handler.
void KiTrap08Handler(CPUState* State)
{
	ENTER_INTERRUPT(IPL_NOINTS);
	
	KeOnDoubleFault(State->rip);
	
	LEAVE_INTERRUPT;
}

// General protection fault handler.
void KiTrap0DHandler(CPUState* State)
{
	ENTER_INTERRUPT(IPL_NOINTS);
	
	KeOnProtectionFault(State->rip);
	
	LEAVE_INTERRUPT;
}

// Page fault handler.
void KiTrap0EHandler(CPUState* State)
{
	ENTER_INTERRUPT(IPL_DPC);
	
	KeOnPageFault(State->rip, State->cr2, State->error_code);
	
	LEAVE_INTERRUPT;
}

// TLB shootdown interrupt.
void KiTrapFDHandler(UNUSED CPUState* State)
{
	CPU* myCPU = KeGetCPU();
	
#ifdef DEBUG2
	SLogMsg("Handling TLB shootdown on CPU %u", myCPU->LapicId);
#endif
	
	for (size_t i = 0; i < myCPU->TlbsLength; i++)
	{
		KeInvalidatePage((void*)(myCPU->TlbsAddress + i * PAGE_SIZE));
	}
	
	KeUnlock(&myCPU->TlbsLock);
	
	HalApicEoi();
}

// Crash IPI (inter processor interrupt).
void HalpOnCrashedCPU();
void KiTrapFEHandler(UNUSED CPUState* State)
{
	// We have received an interrupt from the processor that has crashed. We should crash too!
	
	// we have no plans to return from the interrupt, so ignore the return value of KeRaiseIPL.
	KeRaiseIPL(IPL_NOINTS);
	
	// notify the crash handler that we've stopped
	HalpOnCrashedCPU();
	
	// and stop!!
	KeStopCurrentCPU();
}

// Spurious interrupt.
void KiTrapFFHandler(UNUSED CPUState* State)
{
	HalApicEoi();
}
