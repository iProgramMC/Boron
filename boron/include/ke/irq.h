// Boron - interrupts
#ifndef NS64_KE_IRQ_H
#define NS64_KE_IRQ_H

#include <ke/ipl.h>
#include <arch.h>

// Note! The interrupt handler doesn't ACTUALLY deal in KREGISTERS
// pointers. It just so happens that what we pass in as parameter
// and what we return are PKREGISTERS instances! In reality the pointer
// is actually located within the current thread's stack.
//
// Don't return an instance to PKREGISTERS like I did - since the exit part of the
// trap handler calls KiExitHardwareInterrupt, it's going to go into addresses less
// than that which you return!!
typedef PKREGISTERS(*PKINTERRUPT_HANDLER)(PKREGISTERS);

// Note - This API is to be used during single processor initialization (UP-Init) only.
// Behavior is not defined during MP-Init initialization.
int KeAllocateInterruptVector(KIPL Ipl);

void KeRegisterInterrupt(int Vector, PKINTERRUPT_HANDLER Handler);

void KeSetInterruptIPL(int Vector, KIPL Ipl);

#ifdef KERNEL

// The HAL can't use these directly.
extern
int KiVectorCrash,
    KiVectorTlbShootdown,
    KiVectorDpcIpi;

#else

#define KiVectorCrash        (KeGetSystemInterruptVector(KGSIV_CRASH))
#define KiVectorTlbShootdown (KeGetSystemInterruptVector(KGSIV_TLB_SHOOTDOWN))
#define KiVectorDpcIpi       (KeGetSystemInterruptVector(KGSIV_DPC_IPI))

#endif

// This is destined for use in the HAL.
// The linker doesn't seem to export global value references
// as external dependencies. It's pretty weird.
enum
{
	KGSIV_NONE = 0,
	KGSIV_CRASH,
	KGSIV_TLB_SHOOTDOWN,
	KGSIV_DPC_IPI,
};

int KeGetSystemInterruptVector(int Number);

#endif//NS64_KE_IRQ_H
