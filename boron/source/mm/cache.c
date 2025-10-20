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
	
	for (int i = 0; i < MM_DIRECT_PAGE_COUNT; i++)
		Ccb->Direct[i] = PFN_INVALID;
	
	Ccb->Level1Indirect = PFN_INVALID;
	Ccb->Level2Indirect = PFN_INVALID;
	Ccb->Level3Indirect = PFN_INVALID;
	Ccb->Level4Indirect = PFN_INVALID;
#if MM_INDIRECTION_LEVELS == 5
	Ccb->Level5Indirect = PFN_INVALID;
#endif
}

void MmTearDownCcb(PCCB Ccb)
{
	// This is used when an FCB is freed.  This looks at every cached page,
	// flushes it to disk if it is modified, and frees it.  Then, it frees
	// the indirection levels too.
	
	// TODO
	DbgPrint("TODO: MmTearDownCcb(%p)", Ccb);
}

static MMPFN MiNextEntryCache(MMPFN PfnIndirection, uint64_t* PageOffset, bool AllocateIndirection)
{
	if (IS_BAD_PFN(PfnIndirection))
		return PFN_INVALID;
	
	size_t InPageOffset = *PageOffset & (MM_INDIRECTION_COUNT - 1);
	*PageOffset /= MM_INDIRECTION_COUNT;
	
	// This is slower than it probably should be.
	//
	// However, we have to guard against the PFN getting reclaimed
	// from under our nose before we get to add our own reference to it.
	MmBeginUsingHHDM();
	PMMPFN Indirection = MmGetHHDMOffsetAddrPfn(PfnIndirection);
	
	KIPL Ipl = MiLockPfdb();
	MMPFN Pfn = Indirection[InPageOffset];
	
	if (Pfn == PFN_INVALID && AllocateIndirection)
	{
		bool IsZeroed;
		Pfn = MiAllocatePhysicalPageWithPfdbLocked(&IsZeroed);
		if (Pfn == PFN_INVALID)
		{
			// still invalid, so no dice.
			MiUnlockPfdb(Ipl);
			MmEndUsingHHDM();
			return MM_PFN_OUTOFMEMORY;
		}
		
		// PFN is valid, so clear it
		memset(MmGetHHDMOffsetAddrPfn(Pfn), 0xFF, PAGE_SIZE);
		Indirection[InPageOffset] = Pfn;
	}
	
	if (Pfn != PFN_INVALID)
		MiPageAddReferenceWithPfdbLocked(Pfn);
	
	MiUnlockPfdb(Ipl);
	MmEndUsingHHDM();
	return Pfn;
}

// Input: PfnIndirection - the PFN of the indirection to store to, PageOffset - the offset inside this indirection
static BSTATUS MiAssignEntryToCache(
	PMMPFN Pointer,
	MMPFN Pfn,
	bool IsHhdm,
	PMM_PROTOTYPE_PTE_PTR OutPrototypePtePointer
)
{
	// If we're trying to set a PFN, and a PFN is already set:
	if (!IS_BAD_PFN(*Pointer))
		return STATUS_CONFLICTING_ADDRESSES;
	
	// Assign the entry now.
	if (!IS_BAD_PFN(Pfn))
	{
		*Pointer = Pfn;

		MM_PROTOTYPE_PTE_PTR ProtoPtePtr;
		
	#ifdef IS_32_BIT
		if (IsHhdm)
			ProtoPtePtr = MmGetHHDMOffsetFromAddr(Pointer);
		else
			ProtoPtePtr = MM_VIRTUAL_PROTO_PTE_PTR(Pointer);
	#else
		(void) IsHhdm;
		ProtoPtePtr = Pointer;
	#endif
		
		MmSetPrototypePtePfn(Pfn, ProtoPtePtr);
		
		if (OutPrototypePtePointer)
			*OutPrototypePtePointer = ProtoPtePtr;
	}
	
	return STATUS_SUCCESS;
}

static BSTATUS MiAssignEntryToCacheIndirect(
	MMPFN PfnIndirection,
	uint64_t PageOffset,
	MMPFN Pfn,
	PMM_PROTOTYPE_PTE_PTR OutPrototypePtePointer
)
{
	MmBeginUsingHHDM();
	
	PMMPFN Indirection = MmGetHHDMOffsetAddrPfn(PfnIndirection);
	BSTATUS Result = MiAssignEntryToCache(Indirection + PageOffset, Pfn, false, OutPrototypePtePointer);
	
	MmEndUsingHHDM();
	return Result;
}

static bool MmpEnsureIndirectionExists(PMMPFN Indirection, bool AllocateIndirection)
{
	if (!IS_BAD_PFN(*Indirection))
		return true;
	
	if (!AllocateIndirection)
		return false;
	
	*Indirection = MmAllocatePhysicalPage();
	
	MmBeginUsingHHDM();
	PMMPFN Memory = MmGetHHDMOffsetAddrPfn(*Indirection);
	memset(Memory, 0xFF, PAGE_SIZE);
	MmEndUsingHHDM();
	
	return !IS_BAD_PFN(*Indirection);
}

// Takes in the page offset as passed into MmGetEntryCcb and MmSetEntryCcb.
// However, this does nothing if PageOffset < MM_DIRECT_PAGE_COUNT.
static MMPFN MiWalkCacheUntilIndirection(PCCB Ccb, uint64_t* PageOffset, bool AllocateIndirection)
{
#ifdef DEBUG
	uint64_t OriginalPageOffset = *PageOffset;
#endif
	const MMPFN Failure = AllocateIndirection ? MM_PFN_OUTOFMEMORY : PFN_INVALID;
	
	ASSERT(*PageOffset >= MM_DIRECT_PAGE_COUNT);
	*PageOffset -= MM_DIRECT_PAGE_COUNT;
	
	// Level 1
	if (*PageOffset < MM_DIRECT_PAGE_COUNT)
	{
		if (!MmpEnsureIndirectionExists(&Ccb->Level1Indirect, AllocateIndirection))
			return Failure;
		
		// Well, this is trivial.
		MmPageAddReference(Ccb->Level1Indirect);
		return Ccb->Level1Indirect;
	}
	
	// Level 2
	*PageOffset -= MM_DIRECT_PAGE_COUNT;
	if (*PageOffset < MM_INDIRECTION_COUNT)
	{
		if (!MmpEnsureIndirectionExists(&Ccb->Level2Indirect, AllocateIndirection))
			return Failure;
		
		// This is also kind of trivial although less so.
		return MiNextEntryCache(Ccb->Level2Indirect, PageOffset, AllocateIndirection);
	}
	
	// Level 3
	*PageOffset -= MM_INDIRECTION_COUNT;
	if (*PageOffset < MM_INDIRECTION_COUNT * MM_INDIRECTION_COUNT)
	{
		if (!MmpEnsureIndirectionExists(&Ccb->Level3Indirect, AllocateIndirection))
			return Failure;
		
		MMPFN Pfn3 = MiNextEntryCache(Ccb->Level3Indirect, PageOffset, AllocateIndirection);
		if (IS_BAD_PFN(Pfn3))
			return Pfn3;
		
		MMPFN Pfn2 = MiNextEntryCache(Pfn3, PageOffset, AllocateIndirection);
		MmFreePhysicalPage(Pfn3);
		return Pfn2;
	}
	
	// Level 4
	*PageOffset -= MM_INDIRECTION_COUNT * MM_INDIRECTION_COUNT;
	if (*PageOffset < MM_INDIRECTION_COUNT * MM_INDIRECTION_COUNT * MM_INDIRECTION_COUNT)
	{
		if (!MmpEnsureIndirectionExists(&Ccb->Level4Indirect, AllocateIndirection))
			return Failure;
		
		MMPFN Pfn4 = MiNextEntryCache(Ccb->Level4Indirect, PageOffset, AllocateIndirection);
		if (IS_BAD_PFN(Pfn4))
			return Pfn4;
		
		MMPFN Pfn3 = MiNextEntryCache(Pfn4, PageOffset, AllocateIndirection);
		MmFreePhysicalPage(Pfn4);
		if (IS_BAD_PFN(Pfn3))
			return Pfn3;
		
		MMPFN Pfn2 = MiNextEntryCache(Pfn3, PageOffset, AllocateIndirection);
		MmFreePhysicalPage(Pfn3);
		return Pfn2;
	}
	
#if MM_INDIRECTION_LEVELS == 5
	// Level 5
	*PageOffset -= MM_INDIRECTION_COUNT * MM_INDIRECTION_COUNT * MM_INDIRECTION_COUNT;
	if (*PageOffset < MM_INDIRECTION_COUNT * MM_INDIRECTION_COUNT * MM_INDIRECTION_COUNT * MM_INDIRECTION_COUNT)
	{
		if (!MmpEnsureIndirectionExists(&Ccb->Level5Indirect, AllocateIndirection))
			return Failure;
		
		MMPFN Pfn5 = MiNextEntryCache(Ccb->Level5Indirect, PageOffset, AllocateIndirection);
		if (IS_BAD_PFN(Pfn5))
			return Pfn4;
		
		MMPFN Pfn4 = MiNextEntryCache(Pfn5, PageOffset, AllocateIndirection);
		MmFreePhysicalPage(Pfn5);
		if (IS_BAD_PFN(Pfn4))
			return Pfn4;
		
		MMPFN Pfn3 = MiNextEntryCache(Pfn4, PageOffset, AllocateIndirection);
		MmFreePhysicalPage(Pfn4);
		if (IS_BAD_PFN(Pfn3))
			return Pfn3;
		
		MMPFN Pfn2 = MiNextEntryCache(Pfn3, PageOffset, AllocateIndirection);
		MmFreePhysicalPage(Pfn3);
		return Pfn2;
	}
#endif

	DbgPrint("MiWalkCacheUntilIndirection: Page offset %zu is outside of the supported range.", OriginalPageOffset);
	return PFN_INVALID;
}

MMPFN MmGetEntryCcb(PCCB Ccb, uint64_t PageOffset)
{
	MMPFN Pfn = PFN_INVALID;
	MmLockCcb(Ccb);
	
	if (PageOffset < MM_DIRECT_PAGE_COUNT)
	{
		KIPL Ipl = MiLockPfdb();
		Pfn = Ccb->Direct[PageOffset];
		
		if (!IS_BAD_PFN(Pfn))
			MiPageAddReferenceWithPfdbLocked(Pfn);
		
		MiUnlockPfdb(Ipl);
		goto Done;
	}
	
	Pfn = MiWalkCacheUntilIndirection(Ccb, &PageOffset, false);
	if (!IS_BAD_PFN(Pfn))
	{
		MMPFN Pfn2 = Pfn;
		Pfn = MiNextEntryCache(Pfn2, &PageOffset, false);
		MmFreePhysicalPage(Pfn2);
	}
	
	if (IS_BAD_PFN(Pfn))
		Pfn = PFN_INVALID;
	
Done:
	MmUnlockCcb(Ccb);
	return Pfn;
}

BSTATUS MmSetEntryCcb(PCCB Ccb, uint64_t PageOffset, MMPFN InPfn, PMM_PROTOTYPE_PTE_PTR OutPrototypePtePointer)
{
	BSTATUS Status = STATUS_INSUFFICIENT_MEMORY;
	MmLockCcb(Ccb);
	
	if (PageOffset < MM_DIRECT_PAGE_COUNT)
	{
		Status = MiAssignEntryToCache(&Ccb->Direct[PageOffset], InPfn, false, OutPrototypePtePointer);
		goto Exit;
	}
	
	MMPFN Pfn = MiWalkCacheUntilIndirection(Ccb, &PageOffset, true);
	if (!IS_BAD_PFN(Pfn))
	{
		Status = MiAssignEntryToCacheIndirect(Pfn, PageOffset, InPfn, OutPrototypePtePointer);
		MmFreePhysicalPage(Pfn);
	}
	
Exit:
	MmUnlockCcb(Ccb);
	return Status;
}
