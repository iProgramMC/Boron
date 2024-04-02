/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ke/int.h
	
Abstract:
	This header file defines the interrupt object and
	its manipulation functions.
	
Author:
	iProgramInCpp - 31 March 2024
***/
#pragma once

// N.B. Currently this supports hardware interrupts only.  If needed,
// I will introduce software interrupt objects at a later date.

typedef struct _KINTERRUPT KINTERRUPT, *PKINTERRUPT;

typedef void(*PKSERVICE_ROUTINE)(PKINTERRUPT Interrupt, void* Context);
typedef int(*PKSYNCHRONIZE_ROUTINE)(void* SynchronizeContext);

struct _KINTERRUPT
{
	// The entry into the interrupt dispatch list table.
	LIST_ENTRY Entry;
	
	// Whether KeConnectInterrupt was used on the interrupt.
	bool Connected;
	
	// Whether the vector can be shared by multiple interrupts.
	bool SharedVector;
	
	// The interrupt vector number.
	int Vector;
	
	// The service routine.
	PKSERVICE_ROUTINE ServiceRoutine;
	
	// The service routine context.
	void* ServiceContext;
	
	// The IPL at which the interrupt routine runs.
	KIPL Ipl;
	
	// The spin lock held while the interrupt is running.
	// Used by KeSynchronizeExecution().
	PKSPIN_LOCK SpinLock;
};

//
// Initializes an interrupt object.
//
// Parameters:
// * Interrupt      - Pointer to the interrupt object to initialize.
//
// * ServiceRoutine - The routine to run when an interrupt for the
//                    specified vector is received.
//
// * ServiceContext - A pointer passed into the ServiceRoutine when
//                    it is called.
//
// * SpinLock       - A pointer to a spin lock used to synchronize
//                    with the interrupt object using KeSynchro-
//                    nizeExecution().
//
// * Vector         - The interrupt vector number associated with
//                    this interrupt.
//
// * InterruptIpl   - The IPL of the interrupt source.
//
// * SharedVector   - Whether multiple interrupt objects may be con-
//                    nected to the same interrupt vector.
//
void KeInitializeInterrupt(
	PKINTERRUPT Interrupt,
	PKSERVICE_ROUTINE ServiceRoutine,
	void* ServiceContext,
	PKSPIN_LOCK SpinLock,
	int Vector,
	KIPL InterruptIpl,
	bool SharedVector
);

//
// Connect the interrupt to the interrupt dispatch table.
//
// Returns:
// * TRUE  - If the interrupt was connected successfully
//
// * FALSE - If the interrupt object could not be connected because
//           an interrupt with the same vector and SharedVector == FALSE
//           was already connected.
//
bool KeConnectInterrupt(PKINTERRUPT Interrupt);

//
// Disconnects the interrupt object from the interrupt dispatch table.
//
void KeDisconnectInterrupt(PKINTERRUPT Interrupt);

//
// Execute a routine ensuring that the interrupt is not being executed.
//
// Returns: The return value after the call to Routine.
//
// Parameters:
// * Interrupt      - The interrupt object that is being synchronized with
//
// * SynchronizeIpl - The IPL raised to in order to synchronize with the interrupt.
//                    If lower than the interrupt's IPL, it is ignored.
//
// * Routine, RoutineContext - The function to call, with the specified context.
//
int KeSynchronizeExecution(
	PKINTERRUPT Interrupt,
	KIPL SynchronizeIpl,
	PKSYNCHRONIZE_ROUTINE Routine,
	void* SynchronizeContext
);
