/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ke/irq.h
	
Abstract:
	This header file defines the kernel internal interrupt
	interface.  Device drivers should use the interface
	defined in ke/int.h.
	
Author:
	iProgramInCpp - 28 October 2023
***/
#ifndef NS64_KE_IRQ_H
#define NS64_KE_IRQ_H

#include <ke/ipl.h>
#include <arch.h>

//
// WARNING!
//
// These programming interfaces are NOT to be used for drivers.  They are only designed
// for the kernel and the HAL. Use the interrupt object interface (ke/int.h) in order to
// use interrupts in your device driver.
//

#if defined KERNEL || defined IS_HAL

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
    KiVectorDpcIpi,
    KiVectorApcIpi;

#else

#define KiVectorCrash        (KeGetSystemInterruptVector(KGSIV_CRASH))
#define KiVectorTlbShootdown (KeGetSystemInterruptVector(KGSIV_TLB_SHOOTDOWN))
#define KiVectorDpcIpi       (KeGetSystemInterruptVector(KGSIV_DPC_IPI))
#define KiVectorApcIpi       (KeGetSystemInterruptVector(KGSIV_APC_IPI))

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
	KGSIV_APC_IPI,
};

int KeGetSystemInterruptVector(int Number);

#endif

#endif//NS64_KE_IRQ_H
