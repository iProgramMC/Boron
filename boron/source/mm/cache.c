/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	mm/cache.c
	
Abstract:
	This module implements the cache control block (CCB)
	object for the memory manager.
	
Author:
	iProgramInCpp - 18 August 2024
***/
#include "mi.h"

// TODO: There is probably going to be a lot of contention on the PFDB lock
// from here.  Find a better way.

void MmInitializeCcb(PCCB Ccb)
{
	memset(Ccb, 0, sizeof *Ccb);
	
	KeInitializeMutex(&Ccb->Mutex, MM_CCB_MUTEX_LEVEL);
	MmInitializeSla(&Ccb->Sla);
}

static void MmpCcbFreeEntrySla(MMSLA_ENTRY Entry)
{
	// TODO: remove this page from the standby list if it's there,
	// and put it on the free list.
	(void) Entry;
}

void MmTearDownCcb(PCCB Ccb)
{
	// This is used when an FCB is freed.  This looks at every cached page,
	// flushes it to disk if it is modified, and frees it.  Then, it frees
	// the indirection levels too.
	
	// TODO
	DbgPrint("TODO: MmTearDownCcb(%p)", Ccb);
	MmDeinitializeSla(&Ccb->Sla, MmpCcbFreeEntrySla);
}

static void MmpCcbReferenceSlaEntry(PMMSLA_ENTRY Entry)
{
	// Acquire the PFDB lock to prevent page reclamation.
	//
	// This is the only case where the entry may be modified from right
	// under our noses.
	KIPL Ipl = MiLockPfdb();
	
	// If there is no data after locking, that probably just means
	// this page was reclaimed, or never existed in the first place,
	// so just return.
	if (*Entry == MM_SLA_NO_DATA)
	{
		MiUnlockPfdb(Ipl);
		return;
	}
	
	// Reference this page to prevent it from being reclaimed through the
	// standby list.
	MMPFN Page = (MMPFN) *Entry;
	MiPageAddReferenceWithPfdbLocked(Page);
	
	MiUnlockPfdb(Ipl);
}

MMPFN MmGetEntryCcb(PCCB Ccb, uint64_t PageOffset)
{
	MmLockCcb(Ccb);
	
	MMSLA_ENTRY SlaEntry = MmLookUpEntrySlaEx(
		&Ccb->Sla,
		PageOffset,
		MmpCcbReferenceSlaEntry
	);
	
	MmUnlockCcb(Ccb);
	
	if (SlaEntry == MM_SLA_NO_DATA)
		return PFN_INVALID;
	
	MMPFN Pfn = (MMPFN) SlaEntry;
	return Pfn;
}

BSTATUS MmSetEntryCcb(PCCB Ccb, uint64_t PageOffset, MMPFN InPfn, PMM_PROTOTYPE_PTE_PTR OutPrototypePtePointer)
{
	MmLockCcb(Ccb);
	
	MMSLA_ENTRY SlaEntry = (MMSLA_ENTRY) InPfn;
	
	if (InPfn == PFN_INVALID)
		SlaEntry = MM_SLA_NO_DATA;
	
	MMSLA_ENTRY ReturnValue = MmAssignEntrySlaEx(
		&Ccb->Sla,
		PageOffset,
		SlaEntry,
		OutPrototypePtePointer
	);
	
	MmUnlockCcb(Ccb);
	
	if (ReturnValue == MM_SLA_OUT_OF_MEMORY) {
		DbgPrint("MmSetEntryCcb: MmAssignEntrySlaEx returned MM_SLA_OUT_OF_MEMORY!");
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	if (ReturnValue == MM_SLA_NO_DATA && SlaEntry != MM_SLA_NO_DATA) {
		DbgPrint("MmSetEntryCcb: MmAssignEntrySlaEx(%p) returned MM_SLA_NO_DATA!  InPfn: %d", PageOffset, InPfn);
		return STATUS_INVALID_PARAMETER;
	}
	
	return STATUS_SUCCESS;
}
