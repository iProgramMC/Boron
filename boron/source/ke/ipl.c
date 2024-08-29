/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/ipl.c
	
Abstract:
	This module implements the interrupt priority level
	(IPL) setter and getter.
	
Author:
	iProgramInCpp - 15 September 2023
***/
#include "ki.h"

KIPL KeGetIPL()
{
	bool Restore = KeDisableInterrupts();
	KPRCB* me = KeGetCurrentPRCB();
	KIPL Ipl = me->Ipl;
	KeRestoreInterrupts(Restore);
	return Ipl;
}

// Note! KeRaiseIPL and KeLowerIPL must only be called to manage IPLs on the software side.
// The hardware interrupts modify the IPL by using KeEnterHardwareInterrupt and
// KeExitHardwareInterrupt.

// Note: KeOnUpdateIPL lets the interrupt controller know we're updating the IPL.
// So it's important that we raise the IPL in the PRCB after, and lower it in the
// PRCB before, we let the interrupt controller know.

KIPL KeRaiseIPLIfNeeded(KIPL NewIpl)
{
	KPRCB* Prcb = KeGetCurrentPRCB();
	
	// TODO: Remove the cases where Prcb can be NULL.
	if (!Prcb)
		return NewIpl;
	
	KIPL OldIpl = Prcb->Ipl;
	
	if (NewIpl > OldIpl)
	{
		KeOnUpdateIPL(NewIpl, OldIpl);
		Prcb->Ipl = NewIpl;
	}
	
	return OldIpl;
}

KIPL KeRaiseIPL(KIPL NewIpl)
{
	KPRCB* Prcb = KeGetCurrentPRCB();
	ASSERT(Prcb);
	
	KIPL OldIpl = Prcb->Ipl;
	
	if (OldIpl == NewIpl)
		return OldIpl; // no change
	
	ASSERT(OldIpl < NewIpl);
	
	// TODO: Perhaps it's not even necessary to reprogram the interrupt controller.
	// No hardware interrupt is going to come in at IPL_DPC or below... (well, at
	// least when I wean off my hacky solution of sending the self processor an IPI)
	bool Restore = KeDisableInterrupts();
	KeOnUpdateIPL(NewIpl, OldIpl);
	Prcb->Ipl = NewIpl;
	KeRestoreInterrupts(Restore);
	
	return OldIpl;
}

// similar logic, except we will also call DPCs if needed
KIPL KeLowerIPL(KIPL NewIpl)
{
	KPRCB* Prcb = KeGetCurrentPRCB();
	
	// TODO: Remove the cases where Prcb can be NULL.
	if (!Prcb)
		return NewIpl;
	
	KIPL OldIpl = Prcb->Ipl;
	
	if (OldIpl == NewIpl)
		return OldIpl; // no changes
	
	ASSERT(OldIpl > NewIpl);
	
	// If we fell below DPC level, check if we have any pending events and issue a self IPI.
#if 0
	if (NewIpl < IPL_DPC)
	{
		bool Restore = KeDisableInterrupts();
		
		// TODO: Don't use an IPI to do this. Huge overhead
		if (KeGetPendingEvents())
			HalRequestIpi(0, HAL_IPI_SELF, KiVectorDpcIpi);
		
		KeRestoreInterrupts(Restore);
	}
#endif
	
	// Set the current IPL
	bool Restore = KeDisableInterrupts();
	Prcb->Ipl = NewIpl;
	KeOnUpdateIPL(NewIpl, OldIpl);
	KeRestoreInterrupts(Restore);
	
	// Check if any pending software interrupts are at this level or above
	if (Prcb->PendingSoftInterrupts >> NewIpl)
		KiDispatchSoftwareInterrupts(NewIpl);
	
	return OldIpl;
}
