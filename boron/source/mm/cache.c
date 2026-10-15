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
	if (AtLoadMO(*Entry, ATOMIC_MEMORD_ACQUIRE) == MM_SLA_NO_DATA)
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

// TODO: Basically the same as MmpCcbReferenceSlaEntry, but with additional code
// to return the PFN.
static bool MmpReferenceSlaEntryByOffsetCcb(PMMSLA_ENTRY Entry, PMMPFN OutPfn)
{
	KIPL Ipl = MiLockPfdb();

	if (AtLoadMO(*Entry, ATOMIC_MEMORD_ACQUIRE) == MM_SLA_NO_DATA)
	{
		MiUnlockPfdb(Ipl);
		return false;
	}

	MMPFN Page = (MMPFN) *Entry;
	MiPageAddReferenceWithPfdbLocked(Page);

	MiUnlockPfdb(Ipl);
	*OutPfn = Page;
	return true;
}

BSTATUS MmpForEachEntryByLevelCcb(
	PCCB Ccb,
	MMPFN IndirectPfn,
	size_t IndirectLevel,
	uint64_t StartOffset,
	PCC_FOREACH_FILTER_CALL Filter,
	PCC_FOREACH_PROCESS_CALL ProcessEntry,
	void* FilterContext,
	void* ProcessEntryContext
)
{
	BSTATUS Status = STATUS_SUCCESS;

	// CCB is locked, unlock when calling ProcessEntry.
	ASSERT(IndirectLevel < MM_SLA_INDIRECT_LEVELS);
	if (IndirectLevel == 0)
	{
		for (size_t i = 0; i < MM_SLA_ENTRIES_PER_PAGE; i++)
		{
			MMPFN Pfn;

			MmBeginUsingHHDM();
			PMMSLA_ENTRY EntryAddress = MmGetHHDMOffsetAddr(
				MmPFNToPhysPage(IndirectPfn) + sizeof(MMSLA_ENTRY) * i
			);

			bool Result = MmpReferenceSlaEntryByOffsetCcb(EntryAddress, &Pfn);
			MmEndUsingHHDM();

			if (!Result) {
				continue;
			}

			if (!Filter(Pfn, StartOffset + i, FilterContext)) {
				MmFreePhysicalPage(Pfn);
				continue;
			}

			MmUnlockCcb(Ccb);
			Status = ProcessEntry(Pfn, StartOffset + i, ProcessEntryContext);
			MmFreePhysicalPage(Pfn);
			MmLockCcb(Ccb);

			if (FAILED(Status)) {
				break;
			}
		}

		return Status;
	}

	// More layers of indirection, here we go.
	for (size_t i = 0; i < MM_PFN_ENTRIES_PER_PAGE; i++)
	{
		MMPFN Pfn;

		uint64_t EntriesPerLevel = MM_SLA_ENTRIES_PER_PAGE;
		for (size_t j = 0; j < IndirectLevel; j++) {
			EntriesPerLevel *= MM_PFN_ENTRIES_PER_PAGE;
		}

		MmBeginUsingHHDM();

		PMMPFN PfnPtr = MmGetHHDMOffsetAddr(
			MmPFNToPhysPage(IndirectPfn) + sizeof(MMPFN) * i
		);
		Pfn = *PfnPtr;
		if (Pfn != PFN_INVALID) {
			MmPageAddReference(Pfn);
		}

		MmEndUsingHHDM();

		if (Pfn == PFN_INVALID) {
			continue;
		}

		Status = MmpForEachEntryByLevelCcb(
			Ccb,
			Pfn,
			IndirectLevel - 1,
			StartOffset + EntriesPerLevel * i,
			Filter,
			ProcessEntry,
			FilterContext,
			ProcessEntryContext
		);

		MmFreePhysicalPage(Pfn);

		if (FAILED(Status)) {
			break;
		}
	}

	return Status;
}

BSTATUS MmForEachEntryCcb(
	PCCB Ccb,
	PCC_FOREACH_FILTER_CALL Filter,
	PCC_FOREACH_PROCESS_CALL ProcessEntry,
	void* FilterContext,
	void* ProcessEntryContext
)
{
	BSTATUS Status = STATUS_SUCCESS;

	// SYNCING NOTE: We **unlock** the CCB during ProcessEntry, because it could
	// perform blocking I/O and we don't want to block for the entire duration of
	// the aforementioned I/O.
	//
	// TODO: Don't peer into the MMSLA structure.  However, we kind of need this to
	// give the code some guarantees.
	MmLockCcb(Ccb);

	// Layer 0: direct entries.
	for (size_t i = 0; i < MM_SLA_DIRECT_ENTRY_COUNT; i++)
	{
		MMPFN Pfn = PFN_INVALID;

		if (!MmpReferenceSlaEntryByOffsetCcb(&Ccb->Sla.Direct[i], &Pfn))
			continue;

		if (!Filter(Pfn, i, FilterContext)) {
			MmFreePhysicalPage(Pfn);
			continue;
		}

		MmUnlockCcb(Ccb);
		Status = ProcessEntry(Pfn, i, ProcessEntryContext);
		MmFreePhysicalPage(Pfn);
		MmLockCcb(Ccb);

		if (FAILED(Status)) {
			goto EarlyExit;
		}
	}

	uint64_t StartOffset = MM_SLA_DIRECT_ENTRY_COUNT;
	uint64_t EntriesPerLevel = MM_SLA_ENTRIES_PER_PAGE;
	for (size_t i = 0; i < MM_SLA_INDIRECT_LEVELS; i++)
	{
		Status = MmpForEachEntryByLevelCcb(Ccb,
			Ccb->Sla.Indirect[i],
			i, // Level
			StartOffset,
			Filter,
			ProcessEntry,
			FilterContext,
			ProcessEntryContext
		);

		if (FAILED(Status)) {
			break;
		}

		StartOffset += EntriesPerLevel;
		EntriesPerLevel *= MM_PFN_ENTRIES_PER_PAGE;
	}

EarlyExit:
	MmUnlockCcb(Ccb);
	return Status;
}
