/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	io/filemap.c
	
Abstract:
	This module implements the file object's mappable
	object trait.
	
Author:
	iProgramInCpp - 9 December 2025
***/
#include "iop.h"

//#define FILE_PAGE_FAULT_DEBUG

#ifdef FILE_PAGE_FAULT_DEBUG
#define FPFDbgPrint(...) DbgPrint(__VA_ARGS__)
#else
#define FPFDbgPrint(...)
#endif

// would name it IopGetPageFile but this may make people think I'm talking
// about page files which aren't yet a thing
static BSTATUS IopGetPageFromFile(void* MappableObject, uint64_t SectionOffset, PMMPFN OutPfn)
{
	PFILE_OBJECT FileObject = MappableObject;
	PFCB Fcb = FileObject->Fcb;
	BSTATUS Status;
	
	IO_BACKING_MEM_METHOD BackingMemory = Fcb->DispatchTable->BackingMemory;
	if (BackingMemory)
	{
		IO_STATUS_BLOCK Iosb;
		Status = BackingMemory(&Iosb, Fcb, SectionOffset * PAGE_SIZE);
		
		if (FAILED(Status))
		{
			if (Status != STATUS_OUT_OF_FILE_BOUNDS)
			{
				DbgPrint(
					"IopGetPageFromFile: Backing memory doesn't work for FCB %p and offset %llu! %s",
					Fcb,
					SectionOffset,
					RtlGetStatusString(Status)
				);
				return Status;
			}
			
			// Outside of the file's boundaries.  We should handle this normally using the
			// page cache.  This is because the temporary file system implements files using
			// the BackingMemory call, and sometimes ELF mappings can reach outside the file.
		}
		else
		{
			// NOTE: Here is where driver writers *MUST* ensure they registered the backing memory
			// with the page frame database! (e.g. via MmRegisterMMIOAsMemory).
			//
			// NOTE: The BackingMemory call MUST increasethe reference count of the physical page
			// by one before returning to the caller.
			*OutPfn = MmPhysPageToPFN(Iosb.BackingMemory.PhysicalAddress);
			return STATUS_SUCCESS;
		}
	}
	
	MMPFN Pfn = PFN_INVALID;
	PCCB PageCache = &Fcb->PageCache;
	
	Pfn = MmGetEntryCcb(PageCache, SectionOffset);
	
	// Did we find the PFN already?
	if (Pfn != PFN_INVALID)
	{
		// Yes, we did, so return it.
		MmSetCacheDetailsPfn(Pfn, FileObject->Fcb, SectionOffset * PAGE_SIZE);
		*OutPfn = Pfn;
		
		FPFDbgPrint("%s: hooray! request for offset %llu fulfilled by cached fetch", __func__, SectionOffset);
		return STATUS_SUCCESS;
	}
	
	// No, so the PFN must be read.  Forward to the page fault handler.
	return STATUS_MORE_PROCESSING_REQUIRED;
}

static BSTATUS IopReadPageFromFile(void* MappableObject, uint64_t SectionOffset, PMMPFN OutPfn)
{
	PFILE_OBJECT FileObject = MappableObject;
	PFCB Fcb = FileObject->Fcb;
	PCCB PageCache = &Fcb->PageCache;
	IO_STATUS_BLOCK Iosb;
	
	BSTATUS Status = MmSetEntryCcb(PageCache, SectionOffset, PFN_INVALID, NULL);
	if (FAILED(Status))
	{
		if (Status == STATUS_CONFLICTING_ADDRESSES)
		{
		RedirectToGetPage:
			FPFDbgPrint("%s: Redirecting to GetPage", __func__);
			return IopGetPageFromFile(MappableObject, SectionOffset, OutPfn);
		}
		
		DbgPrint(
			"%s: Failed to set empty entry to CCB at %lld: %s",
			__func__,
			SectionOffset,
			RtlGetStatusString(Status)
		);
		
		return Status;
	}
	
	// Allocate a PFN now, do a read, and then put it into the CCB.
	MMPFN Pfn = MmAllocatePhysicalPage();
	if (Pfn == PFN_INVALID)
	{
		DbgPrint("%s: out of memory because a page frame couldn't be allocated", __func__);
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	// Okay.  Let's perform the IO on this PFN.
	MDL_ONEPAGE Mdl;
	MmInitializeSinglePageMdl(&Mdl, Pfn, MDL_FLAG_WRITE);
	
	Status = IoPerformPagingRead(
		&Iosb,
		FileObject,
		&Mdl.Base,
		SectionOffset * PAGE_SIZE
	);
	
	MmFreeMdl(&Mdl.Base);
	
	if (FAILED(Status))
	{
		// Ran out of memory trying to read this file, or a hardware I/O error occurred.
		DbgPrint(
			"%s: cannot fulfill request because I/O failed with code %d (%s)",
			__func__,
			Status,
			RtlGetStatusString(Status)
		);
		MmFreePhysicalPage(Pfn);
		return Status;
	}
	
	// The I/O operation has succeeded.
	// It's time to put this in the CCB and then map.
	Status = MmSetEntryCcb(PageCache, SectionOffset, Pfn, NULL);
	if (FAILED(Status))
	{
		MmFreePhysicalPage(Pfn);
		
		if (Status == STATUS_CONFLICTING_ADDRESSES)
		{
			// Need to throw away our hard work as someone finished it before us.
			DbgPrint("%s: CCB entry was found to be assigned by the time IO was made, refaulting (2)", __func__);
			goto RedirectToGetPage;
		}
		else
		{
			// Out of memory.  Did someone remove our prior allocation?
			// Well, we've got almost no choice but to throw away our result.
			//
			// (We could instead beg the memory manager for more memory, but
			// I don't feel like writing that right now.)
			ASSERT(Status == STATUS_INSUFFICIENT_MEMORY);
			DbgPrint("%s: out of memory because CCB entry couldn't be allocated (2)", __func__);
		}
		
		return Status;
	}
	
	// Success! This is the right PFN. Now that we've assigned it, it's time to return it.
	MmSetCacheDetailsPfn(Pfn, FileObject->Fcb, SectionOffset * PAGE_SIZE);
	*OutPfn = Pfn;
	
	FPFDbgPrint("%s: hooray! request fulfilled by I/O read", __func__);
	return STATUS_SUCCESS;
}

static BSTATUS IopPrepareWriteFile(void* MappableObject, uint64_t SectionOffset)
{
	(void) MappableObject;
	(void) SectionOffset;
	
	// Files are shared, so no special programming needed.
	return STATUS_SUCCESS;
}

MAPPABLE_DISPATCH_TABLE IopFileObjectMappableDispatch =
{
	.GetPage = IopGetPageFromFile,
	.ReadPage = IopReadPageFromFile,
	.PrepareWrite = IopPrepareWriteFile,
};
