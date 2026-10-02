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

// Resets a view entry to its default, uninitialized state.
void MmpResetViewEntry(PMMVIEW View, size_t Index)
{
	PMMVIEW_ENTRY Entry = &View->Entries[Index];
	
	if (Entry->Pfn != 0)
	{
		MmFreePhysicalPage(Entry->Pfn);
		Entry->Pfn = 0;
	}
	
	Entry->LongEntry = 0;
	Entry->Committed = View->Flags.Committed;
	Entry->CopyOnWrite = View->Flags.Private;
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
BSTATUS MiResolveViewFault(PMMVIEW View, size_t ViewOffset, int Intent, PMMPFN OutPfn)
{
	MMPFN Pfn;
	BSTATUS Status = STATUS_SUCCESS;
	
	ViewOffset &= ~(PAGE_SIZE - 1);
	size_t ViewIndex = ViewOffset / PAGE_SIZE;
	uint64_t SectionOffset = (View->SectionOffset + ViewOffset) / PAGE_SIZE;
	
	// Check if the view has a PFN filled in already.
	PMMVIEW_ENTRY Entry = &View->Entries[ViewIndex];
	if (Entry->Pfn == 0)
	{
		if (!Entry->Committed)
		{
			DbgPrint("MiResolveViewFault(%p, %x): Page not committed.", View, ViewOffset);
			return STATUS_ACCESS_VIOLATION;
		}
		
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
			
			ASSERT(Pfn != 0 && "The returned PFN shouldn't be zero.");
			
			// Just set CopyOnWrite to 0, because there's nothing to copy really.
			Entry->CopyOnWrite = false;
			Entry->Pfn = Pfn;
			
			*OutPfn = Pfn;
			
			DbgPrint("MiResolveViewFault(%p, %x): Filled in PFN %u from anonymous memory.", View, ViewOffset, Pfn);
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
		ASSERT(Pfn != 0 && "The returned PFN shouldn't be zero.");
		Entry->Pfn = Pfn;
		
		DbgPrint(
			"MiResolveViewFault(%p, %x): Filled in PFN %u from backing object %p.",
			View,
			ViewOffset,
			Pfn,
			View->BackingObject
		);
	}
	
	ASSERT(Entry->Pfn != 0);
	
	// There is a PFN, check if the intent is write.
	if (Intent != PAGE_WRITE)
	{
		*OutPfn = Entry->Pfn;
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
	MmFreePhysicalPage(Pfn);
	
	Entry->CopyOnWrite = false;
	
	*OutPfn = NewPfn;
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

typedef struct
{
	void* BackingObject;
	uint64_t SectionOffset;
	size_t SizePages;
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
	
	View->BackingObject = ObReferenceObjectByPointer(CreateContext->BackingObject);
	
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
	
	Status = MmpInitializeViewObject(OutObject, &CreateContext);
	ASSERT(SUCCEEDED(Status));
	
	*OutView = OutObject;
	return STATUS_SUCCESS;
}
