/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	mm/view2.c
	
Abstract:
	This module implements the memory manager View object.
	
    A view represents a list of page references (optionally pertaining to a mapped
    object), with permissions and additional flags.  These represents a machine-
    independent list of pages, which can later be converted to the actual page
    tables the processor will use.
    
    This object is equivalent to the UVM memory manager's "amap" object.

Author:
	iProgramInCpp - 3 April 2025
***/
#include "mi.h"
#include <ex.h>
#include <io.h>

#define VIEW_PFN_INVALID (0)

static int MmpCalculatePagePermissions(PMMVIEW_ENTRY Entry)
{
	int Permissions = Entry->Permissions;
	
	if (Entry->CopyOnWrite)
		Permissions &= ~PAGE_WRITE;
	
	return Permissions;
}

#ifndef DEBUG

#define MmpEnsureBoundsView(View, Index, SizePages)

#else

static void MmpEnsureBoundsView(PMMVIEW View, uintptr_t Index, size_t SizePages)
{
	if (Index >= View->SizePages ||
		Index + SizePages > View->SizePages ||
		Index + SizePages < Index)
	{
		KeCrash(
			"MmpEnsureBoundsView: Bad indices passed.  Index: %zu, SizePages: %zu, "
			"View->SizePages: %zu",
			Index,
			SizePages,
			View->SizePages
		);
	}
}

#endif // DEBUG

// Resets a view entry to its default, uninitialized state.
void MmpResetViewEntry(PMMVIEW View, size_t Index)
{
	PMMVIEW_ENTRY Entry = &View->Entries[Index];
	
	if (Entry->Pfn != VIEW_PFN_INVALID)
	{
		MmFreePhysicalPage(Entry->Pfn);
		Entry->Pfn = VIEW_PFN_INVALID;
	}
	
	Entry->LongEntry = 0;
	Entry->Committed = View->Flags.Committed;
	Entry->CopyOnWrite = View->Flags.Private;
	Entry->Permissions = View->Flags.Permissions;
}

void* MmGetBackingObjectView(PMMVIEW View)
{
	return View->BackingObject
		? ObReferenceObjectByPointer(View->BackingObject)
		: NULL;
}

uint64_t MmGetSectionOffsetView(PMMVIEW View)
{
	return View->SectionOffset;
}

//
// Resolves a page fault on a view.
//
// Parameters:
//    View - The view itself.
//
//    ViewOffset - The offset within the view, in bytes.  NOT the same as
//                 the offset into the backing object.
//
//    Intent - One of PAGE_READ, PAGE_WRITE, or PAGE_EXECUTE.  Represents
//             the intent of the page fault (reading, writing, or executing).
//             This is not a bitset.
//
//    OutPfn - The PFN will be returned here if the resolution succeeded.
//             The view will also be updated, if necessary.
//
//    OutPermissions - The permissions allowed on this PFN.  If CopyOnWrite
//                     is set, the PAGE_WRITE bit will be cleared.
//
BSTATUS MiResolveViewFault(PMMVIEW View, size_t ViewOffset, int Intent, PMMPFN OutPfn, int* OutPermissions)
{
	MMPFN Pfn;
	BSTATUS Status = STATUS_SUCCESS;
	int Permissions = 0;
	
	ViewOffset &= ~(PAGE_SIZE - 1);
	size_t ViewIndex = ViewOffset / PAGE_SIZE;
	uint64_t SectionOffset = (View->SectionOffset + ViewOffset) / PAGE_SIZE;
	
	PMMVIEW_ENTRY Entry = &View->Entries[ViewIndex];
	
	// If the intended operation is not allowed:
	if (!Entry->Committed)
	{
		DbgPrint("MiResolveViewFault(%p, %x): Page not committed.", View, ViewOffset);
		return STATUS_ACCESS_VIOLATION;
	}
	
	if (~Entry->Permissions & Intent)
	{
		DbgPrint(
			"MiResolveViewFault(%p, %x): Intent %d doesn't match permission bitmask %d.",
			View,
			ViewOffset,
			Intent,
			Entry->Permissions
		);
		return STATUS_ACCESS_VIOLATION;
	}
	
	// Check if the view has a PFN filled in already.
	if (Entry->Pfn == VIEW_PFN_INVALID)
	{
		// Committed.  Try and bring it in.
		if (!View->BackingObject)
		{
			// Anonymous memory: just allocate a zero-filled PFN and return it.
			//
			// NOTE: In the future, we will implement commit tracking, which will,
			// if not prevent, make such occasions more rare.
			Pfn = MmAllocatePhysicalPage();
			if (Pfn == PFN_INVALID)
			{
				DbgPrint(
					"MiResolveViewFault(%p, %x): Could not allocate anonymous page.  "
					"Out of memory.",
					View,
					ViewOffset
				);
				return STATUS_INSUFFICIENT_MEMORY;
			}
			
			ASSERT(Pfn != VIEW_PFN_INVALID && "The returned PFN shouldn't be equal to VIEW_PFN_INVALID.");
			
			// Just set CopyOnWrite to 0, because there's nothing to copy really.
			Entry->CopyOnWrite = false;
			Entry->Pfn = Pfn;
			
			*OutPermissions = MmpCalculatePagePermissions(Entry);
			*OutPfn = Pfn;
			
			// We're keeping a reference to this PFN for ourselves.
			MmPageAddReference(Pfn);
			
			DbgPrint(
				"MiResolveViewFault(%p, %x): Filled in PFN %u from anonymous memory.",
				View,
				ViewOffset,
				Pfn
			);
			return STATUS_SUCCESS;
		}
		
		// Backed by a section or file.  Request the page now.
		//
		// NOTE: This can return STATUS_MORE_PROCESSING_REQUIRED - requires calling
		// MmReadPageMappable, which is done in the routine below.
		Status = MmGetPageMappable(
			View->BackingObject,
			SectionOffset,
			&Pfn
		);
		
		if (Status == STATUS_MORE_PROCESSING_REQUIRED)
		{
			DbgPrint("MiResolveViewFault(%p, %x): More processing required.", View, ViewOffset);
			return Status;
		}
		
		if (FAILED(Status))
		{
			DbgPrint(
				"MiResolveViewFault(%p, %x): Can't get page from mappable object. %s",
				View,
				ViewOffset,
				RtlGetStatusString(Status)
			);
			return Status;
		}
		
		// Page obtained.  Put it in the view now.
		// MmGetPageMappable gave us a reference, so put it in, and duplicate it later
		// for the caller.
		ASSERT(Pfn != VIEW_PFN_INVALID && "The returned PFN shouldn't be equal to VIEW_PFN_INVALID.");
		Entry->Pfn = Pfn;
		
		DbgPrint(
			"MiResolveViewFault(%p, %x): Filled in PFN %u from backing object %p.",
			View,
			ViewOffset,
			Pfn,
			View->BackingObject
		);
	}
	
	ASSERT(Entry->Pfn != VIEW_PFN_INVALID);
	
	// There is a PFN, check if the intent is write and this page is not for copy-on-write.
	if (Intent != PAGE_WRITE || !Entry->CopyOnWrite)
	{
		// The caller receives a reference to this page.
		MmPageAddReference(Entry->Pfn);
		*OutPfn = Entry->Pfn;
		*OutPermissions = MmpCalculatePagePermissions(Entry);
		
		DbgPrint(
			"MiResolveViewFault(%p, %x): Return PFN %u from read/execute.",
			View,
			ViewOffset,
			Entry->Pfn
		);
		return STATUS_SUCCESS;
	}
	
	// The intent is write.  We need to copy the existing page and return it.
	MMPFN NewPfn = MmAllocatePhysicalPage();
	if (NewPfn == PFN_INVALID)
	{
		DbgPrint(
			"MiResolveViewFault(%p, %x): Could not allocate anonymous page for copy-on-write.  "
			"Out of memory.",
			View,
			ViewOffset
		);
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	Pfn = Entry->Pfn;
	
	MmBeginUsingHHDM();
	
#ifdef IS_32_BIT
	memcpy(View->TemporaryPageSizeBuffer, MmGetHHDMOffsetAddr(MmPFNToPhysPage(Pfn)), PAGE_SIZE);
	memcpy(MmGetHHDMOffsetAddr(MmPFNToPhysPage(NewPfn)), View->TemporaryPageSizeBuffer, PAGE_SIZE);
#else
	memcpy(
		MmGetHHDMOffsetAddr(MmPFNToPhysPage(NewPfn)),
		MmGetHHDMOffsetAddr(MmPFNToPhysPage(Pfn)),
		PAGE_SIZE
	);
#endif
	
	MmEndUsingHHDM();
	
	Entry->Pfn = NewPfn;
	MmPageAddReference(NewPfn);
	MmFreePhysicalPage(Pfn);
	
	Entry->CopyOnWrite = false;
	Permissions = MmpCalculatePagePermissions(Entry);
	
	*OutPfn = NewPfn;
	*OutPermissions = Permissions;
	return STATUS_SUCCESS;
}

//
// Perform additional processing during the resolution of a view fault.
//
// This function is meant to be called with memory management locks released,
// to allow other memory management functions to execute during this call,
// because this call may perform blocking I/O.
//
// Parameters:
//    View - The view itself.
//
//    ViewOffset - The offset within the view, in bytes.  NOT the same as
//                 the offset into the backing object.
//
BSTATUS MiPerformAdditionalProcessingForViewFault(PMMVIEW View, size_t ViewOffset)
{
	// NOTE: This function can be called anytime, even if the VAD mutex isn't
	// locked, as long as the view object is valid (so make sure to acquire a
	// reference to it before trying to do this)
	//
	// Page fault handler: When returning from this function, return STATUS_REFAULT
	// to retry the fault with the page loaded in.
	BSTATUS Status = STATUS_SUCCESS;
	
	if (!View->BackingObject) {
		// There is no additional processing to be performed.
		return STATUS_SUCCESS;
	}
	
	ViewOffset &= ~(PAGE_SIZE - 1);
	
	uint64_t SectionOffset = (View->SectionOffset + ViewOffset) / PAGE_SIZE;
	
	MMPFN Pfn = PFN_INVALID;
	Status = MmReadPageMappable(
		View->BackingObject,
		SectionOffset,
		&Pfn
	);
	
	if (SUCCEEDED(Status)) {
		// It returned a PFN, but we don't need it.
		MmFreePhysicalPage(Pfn);
	}
	
	return Status;
}

//
// Set the backing page as modified, if required.
//
// Parameters:
//    View - The view itself.
//
//    ViewOffset - The offset within the view, in bytes.  NOT the same as
//                 the offset into the backing object.
//
BSTATUS MiSetPageModifiedView(PMMVIEW View, size_t ViewOffset)
{
	if (!View->BackingObject) {
		// There is no additional processing to be performed.
		return STATUS_SUCCESS;
	}
	
	ViewOffset &= ~(PAGE_SIZE - 1);
	uint64_t SectionOffset = (View->SectionOffset + ViewOffset) / PAGE_SIZE;
	
	return MmSetPageModifiedMappable(
		View->BackingObject,
		SectionOffset
	);
}

BSTATUS MmCommitView(PMMVIEW View, uintptr_t Offset, size_t SizePages, int Permissions)
{
	// Commit: all we need to do is mark pages as committed, for now.
	//
	// TODO: attempt to charge commit, and if that doesn't work, return an overcommitment error.
	// We don't do commit tracking for now.  (For the record, this is why this function doesn't
	// just return void.)
	
	uintptr_t Index = Offset / PAGE_SIZE;
	MmpEnsureBoundsView(View, Index, SizePages);
	
	for (size_t i = 0; i < SizePages; i++)
	{
		PMMVIEW_ENTRY Entry = &View->Entries[Index + i];
		
		if (Entry->Committed)
			continue;
		
		Entry->Pfn = VIEW_PFN_INVALID;
		Entry->Committed = true;
		Entry->Permissions = Permissions;
		Entry->CopyOnWrite = View->Flags.Private;
	}
	
	return STATUS_SUCCESS;
}

void MmDecommitView(PMMVIEW View, uintptr_t Offset, size_t SizePages)
{
	// Decommit: all we need to do is mark pages as decommitted, for now.
	//
	// TODO: Uncharge commit (which also can't fail).
	
	uintptr_t Index = Offset / PAGE_SIZE;
	MmpEnsureBoundsView(View, Index, SizePages);
	
	for (size_t i = 0; i < SizePages; i++)
	{
		PMMVIEW_ENTRY Entry = &View->Entries[Index + i];
		
		if (!Entry->Committed)
			continue;
		
		// free physical page reference, if needed.
		if (Entry->Pfn != VIEW_PFN_INVALID)
		{
			MmFreePhysicalPage(Entry->Pfn);
			Entry->Pfn = VIEW_PFN_INVALID;
		}
		
		Entry->Committed = false;
		Entry->Permissions = 0;
		Entry->CopyOnWrite = 0;
	}
}

void MmCopyOnWriteView(PMMVIEW View)
{
	// TODO: should we charge commit again? How much exactly?
	// Should this function be able to fail? (Rolling everything back
	// would be kind of tedious, though)
	
	for (size_t i = 0; i < View->SizePages; i++)
	{
		PMMVIEW_ENTRY Entry = &View->Entries[i];
		
		if (!Entry->Committed)
			continue;
		
		Entry->CopyOnWrite = true;
	}
}

BSTATUS MmQueryView(
	PMMVIEW View,
	uintptr_t Offset,
	PMMVIEW_ENTRY OutEntryFlags,
	uintptr_t* OutBaseOffset,
	size_t* OutRegionSizePages
)
{
	uintptr_t Index = Offset / PAGE_SIZE;
	MmpEnsureBoundsView(View, Index, 1);
	ASSERT(View->SizePages != 0);
	
	uintptr_t StartIndex = Index, EndIndex = Index;
	
	MMVIEW_ENTRY TargetEntry = View->Entries[Index];
	
	// Look for each edge.  Basically, we need to compare committed status and permissions.
	// As long as they are the same, we can expand.
	while (true)
	{
		if (View->Entries[StartIndex].Committed != TargetEntry.Committed ||
			View->Entries[StartIndex].Permissions != TargetEntry.Permissions) {
			StartIndex++;
			break;
		}
		
		if (StartIndex == 0)
			break;
		
		StartIndex--;
	}
	
	while (true)
	{
		if (View->Entries[EndIndex].Committed != TargetEntry.Committed ||
			View->Entries[EndIndex].Permissions != TargetEntry.Permissions) {
			EndIndex--;
			break;
		}
		
		if (EndIndex == View->SizePages - 1)
			break;
		
		EndIndex++;
	}
	
	// Edges found.  Now, return the actual data.
	*OutBaseOffset = StartIndex * PAGE_SIZE;
	*OutRegionSizePages = EndIndex - StartIndex + 1;
	
#ifdef DEBUG
	TargetEntry.Pfn = 0;
#endif

	TargetEntry.CopyOnWrite = View->Flags.Private;
	
	*OutEntryFlags = TargetEntry;
	return STATUS_SUCCESS;
}

typedef struct
{
	void* BackingObject;
	uint64_t SectionOffset;
	size_t SizePages;
	int CommitPermissions;
	bool Private;
	bool Commit;
}
MMVIEW_CREATE_CONTEXT, *PMMVIEW_CREATE_CONTEXT;

void MmDeleteViewObject(void* ViewPtr)
{
	PMMVIEW View = ViewPtr;
	
	for (size_t i = 0; i < View->SizePages; i++)
	{
		MmpResetViewEntry(View, i);
	}
	
	MmFreePool(View->Entries);
	
	if (View->BackingObject) {
		ObDereferenceObject(View->BackingObject);
	}
	
#ifdef IS_32_BIT
	MmFreePool(View->TemporaryPageSizeBuffer);
#endif
}

BSTATUS MmpInitializeViewObject(void* ViewPtr, void* Context)
{
	PMMVIEW View = ViewPtr;
	PMMVIEW_CREATE_CONTEXT CreateContext = Context;
	
	View->Entries = MmAllocatePool(POOL_NONPAGED, sizeof(MMVIEW_ENTRY) * CreateContext->SizePages);
	if (!View->Entries)
	{
		DbgPrint("MmInitializeViewObject: Could not allocate entry buffer -- out of memory.");
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	View->SizePages = CreateContext->SizePages;
	View->Flags.Private = CreateContext->Private;
	View->Flags.Committed = CreateContext->Commit;
	View->Flags.Permissions = CreateContext->CommitPermissions;
	
	if (CreateContext->BackingObject) {
		View->BackingObject = ObReferenceObjectByPointer(CreateContext->BackingObject);
	}
	else {
		View->BackingObject = NULL;
	}
	
	// Initialize all the entries within the list.
	for (size_t i = 0; i < View->SizePages; i++)
	{
		MmpResetViewEntry(View, i);
	}
	
#ifdef IS_32_BIT
	View->TemporaryPageSizeBuffer = MmAllocatePool(POOL_NONPAGED, PAGE_SIZE);
#endif
	
	return STATUS_SUCCESS;
}

BSTATUS MmCreateView(
	void* BackingObject,
	uint64_t SectionOffset,
	size_t SizePages,
	bool Private,
	bool Commit,
	int CommitPermissions,
	PMMVIEW* OutView
)
{
	void* OutObject;
	BSTATUS Status;
	
	// TODO: Do we even need reference counting here?  It's possible we don't.
	Status = ObCreateObject(
		&OutObject,
		NULL, // ParentDirectory
		MmViewObjectType,
		NULL, // ObjectName
		OB_FLAG_NO_DIRECTORY,
		NULL, // ParseContext
		sizeof(MMVIEW)
	);
	
	if (FAILED(Status))
		return Status;
	
	MMVIEW_CREATE_CONTEXT CreateContext;
	CreateContext.BackingObject = BackingObject;
	CreateContext.SectionOffset = SectionOffset;
	CreateContext.SizePages = SizePages;
	CreateContext.Private = Private;
	CreateContext.Commit = Commit;
	CreateContext.CommitPermissions = CommitPermissions;
	
	Status = MmpInitializeViewObject(OutObject, &CreateContext);
	ASSERT(SUCCEEDED(Status));
	
	*OutView = OutObject;
	return STATUS_SUCCESS;
}

BSTATUS MmCloneView(PMMVIEW InView, PMMVIEW* OutView)
{
	ASSERT(InView);
	
	BSTATUS Status;
	PMMVIEW View;
	
	Status = MmCreateView(
		InView->BackingObject,
		InView->SectionOffset,
		InView->SizePages,
		InView->Flags.Private,
		InView->Flags.Committed,
		InView->Flags.Permissions,
		&View
	);
	
	if (FAILED(Status))
		return Status;
	
	// Now copy every entry as copy-on-write.
	for (size_t i = 0; i < InView->SizePages; i++)
	{
		PMMVIEW_ENTRY InEntry = &InView->Entries[i];
		PMMVIEW_ENTRY OutEntry = &View->Entries[i];
		
		// copy bit for bit at first...
		OutEntry->LongEntry = InEntry->LongEntry;
		OutEntry->CopyOnWrite = true;
		
		// but add an extra reference to the PFN so that we have a copy of it too
		// (TODO: charge commit here?)
		if (OutEntry->Pfn != VIEW_PFN_INVALID && OutEntry->Committed)
		{
			MmPageAddReference(OutEntry->Pfn);
		}
	}
	
	*OutView = View;
	return STATUS_SUCCESS;
}
