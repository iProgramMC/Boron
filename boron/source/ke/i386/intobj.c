/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/i386/intobj.c
	
Abstract:
	This module implements the interrupt object for
	the i386 platform.
	
	N.B. The interval timer and DPC dispatch functions
	do not use the interrupt object.
	
Author:
	iProgramInCpp - 14 October 2025
***/
#include <ke.h>
#include <hal.h>

extern int8_t KiTrapIplList[];  // trap.asm
extern void*  KiTrapCallList[]; // trap.asm

typedef struct
{
	LIST_ENTRY List;
	KSPIN_LOCK Lock;
}
INTERRUPT_LIST, *PINTERRUPT_LIST;

static INTERRUPT_LIST KiInterruptList[256];

static PKREGISTERS KiInterruptDispatch(PKREGISTERS Regs)
{
	int Number = Regs->IntNumber;
	PINTERRUPT_LIST InterruptList = &KiInterruptList[Number];
	KIPL Ipl;
	
	KeAcquireSpinLock(&InterruptList->Lock, &Ipl);
	
	for (PLIST_ENTRY Entry = InterruptList->List.Flink;
	     Entry != &InterruptList->List;
		 Entry = Entry->Flink)
	{
		PKINTERRUPT Interrupt = CONTAINING_RECORD(Entry, KINTERRUPT, Entry);
		KIPL Unused;
		KeAcquireSpinLock(Interrupt->SpinLock, &Unused);
		
		Interrupt->ServiceRoutine(Interrupt, Interrupt->ServiceContext);
		
		KeReleaseSpinLock(Interrupt->SpinLock, Unused);
	}
	
	KeReleaseSpinLock(&InterruptList->Lock, Ipl);
	
	// Acknowledge the interrupt.
	HalEndOfInterrupt();
	
	// No change in registers.
	return Regs;
}

INIT
void KiInitializeInterruptSystem()
{
	for (int i = 0; i < 256; i++)
	{
		InitializeListHead(&KiInterruptList[i].List);
		KeInitializeSpinLock(&KiInterruptList[i].Lock);
	}
}

void KeInitializeInterrupt(
	PKINTERRUPT Interrupt,
	PKSERVICE_ROUTINE ServiceRoutine,
	void* ServiceContext,
	PKSPIN_LOCK SpinLock,
	int Vector,
	KIPL InterruptIpl,
	bool SharedVector)
{
	ASSERT((Vector >> 4) == InterruptIpl && "The interrupt vector must match its IPL on AMD64.");
	ASSERT(InterruptIpl > IPL_DPC   && "The caller may not override this IPL");
	ASSERT(InterruptIpl < IPL_CLOCK && "The caller may not override this IPL");
	
	Interrupt->Connected = false;
	Interrupt->SharedVector = SharedVector;
	Interrupt->Vector = Vector;
	Interrupt->Ipl = InterruptIpl;
	Interrupt->ServiceRoutine = ServiceRoutine;
	Interrupt->ServiceContext = ServiceContext;
	Interrupt->SpinLock = SpinLock;
}

bool KeConnectInterrupt(PKINTERRUPT Interrupt)
{
	ASSERT(!Interrupt->Connected && "It's already connected!");
	
	PINTERRUPT_LIST InterruptList = &KiInterruptList[Interrupt->Vector];
	
	KIPL Ipl, IplUnused;
	Ipl = KeRaiseIPL(Interrupt->Ipl);
	KeAcquireSpinLock(&InterruptList->Lock, &IplUnused);
	
	// Check if the vector may be shared.
	if (!IsListEmpty(&InterruptList->List))
	{
		// If the head has SharedVector == false, return false here.
		PKINTERRUPT Head = CONTAINING_RECORD(InterruptList->List.Flink, KINTERRUPT, Entry);
		
		if (!Head->SharedVector)
		{
			KeReleaseSpinLock(&InterruptList->Lock, IplUnused);
			KeLowerIPL(Ipl);
			return false;
		}
	}
	
	// Connect the interrupt now.
	InsertTailList(&InterruptList->List, &Interrupt->Entry);
	Interrupt->Connected = true;
	
	// In case this wasn't already done, wire up the interrupt
	// dispatcher routine we specified.
	KeRegisterInterrupt(Interrupt->Vector, KiInterruptDispatch);
	
	KeReleaseSpinLock(&InterruptList->Lock, IplUnused);
	KeLowerIPL(Ipl);
	
	return true;
}

void KeDisconnectInterrupt(PKINTERRUPT Interrupt)
{
	ASSERT(Interrupt->Connected && "You need to have connected the interrupt to disconnect it!");
	
	PINTERRUPT_LIST InterruptList = &KiInterruptList[Interrupt->Vector];
	KIPL Ipl, IplUnused;
	Ipl = KeRaiseIPL(Interrupt->Ipl);
	KeAcquireSpinLock(&InterruptList->Lock, &IplUnused);
	
	// Disconnect the interrupt now.
	RemoveEntryList(&Interrupt->Entry);
	Interrupt->Connected = false;
	
	KeReleaseSpinLock(&InterruptList->Lock, IplUnused);
	KeLowerIPL(Ipl);
}

int KeSynchronizeExecution(
	PKINTERRUPT Interrupt,
	KIPL SynchronizeIpl,
	PKSYNCHRONIZE_ROUTINE Routine,
	void* SynchronizeContext)
{
	if (SynchronizeIpl < Interrupt->Ipl)
		SynchronizeIpl = Interrupt->Ipl;
	
	KIPL Ipl, IplUnused;
	Ipl = KeRaiseIPL(SynchronizeIpl);
	KeAcquireSpinLock(Interrupt->SpinLock, &IplUnused);
	
	int Result = Routine(SynchronizeContext);
	
	KeReleaseSpinLock(Interrupt->SpinLock, IplUnused);
	KeLowerIPL(Ipl);
	
	return Result;
}
