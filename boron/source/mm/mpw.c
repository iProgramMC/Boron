/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/mpw.c
	
Abstract:
	This module implements the Boron modified page writer and
	its utilitary functions.
	
Author:
	iProgramInCpp - 10 August 2025
***/
#include "mi.h"
#include <io.h>
#include <ps.h>

// The modified page writer system will be implemented in two halves:
//
// - The modified page writer worker, which writes modified file backed pages
// - The pagefile writer worker, which writes pages to page files
//
// This distinction is important because the modified page writer can cause
// page faults which can result in demand for free pages, but there could be
// no free pages available, which would result in a deadlock.  Separating
// the two systems allows the modified page writer to block without affecting
// ongoing page file write operations.

KEVENT MmModifiedPageWriterEvent;

static BSTATUS MiFlushModifiedPage(MMPFN Pfn)
{
	PMMPFDBE Pfdbe = MmGetPageFrameFromPFN(Pfn);
	
	PFCB Fcb = PFDBE_Fcb(Pfdbe);
	uint64_t Offset = PFDBE_Offset(Pfdbe) * PAGE_SIZE;
	
	return IoPerformModifiedPageWrite(Fcb, Pfn, Offset);
}

NO_RETURN
void MmModifiedPageWriterWorker(UNUSED void* Context)
{
	BSTATUS Status;

	while (true)
	{
		Status = KeWaitForSingleObject(
			&MmModifiedPageWriterEvent,
			false,
			TIMEOUT_INFINITE,
			MODE_KERNEL
		);
		
		KIPL Ipl = MiLockPfdb();
		
		while (true)
		{
			MMPFN Pfn = MiRemoveOneModifiedPfn();
			if (Pfn == PFN_INVALID)
			{
				MiUnlockPfdb(Ipl);
				break;
			}
			
			PMMPFDBE Pfdbe = MmGetPageFrameFromPFN(Pfn);
			
			if (!Pfdbe->Modified)
			{
				DbgPrint("MPW: Pfn %d was in modified list but isn't modified?", Pfn);
				MiTransformPageToStandbyPfn(Pfn);
				continue;
			}
			
			// Clear the modified flag, and increase the reference count of the page by one.
			// This is so that in the event that the page is freed with MmFreePhysicalPage,
			// the page won't be added back to the modified list.
			Pfdbe->RefCount++;
			Pfdbe->Modified = false;
			
			// The PFDBE is modified, start writing its contents to memory.  Then, transform the page
			// into standby (if it wasn't faulted in afterwards), and move on to the next page.
			MiUnlockPfdb(Ipl);
			
			// As of this point, this PFDBE has a reference count of zero, its type is PF_TYPE_USED,
			// and the only reference to it is a weak one in the page cache. Now, since we have
			// unlocked the page frame database lock, additional references may be added to this page.
			// This doesn't matter.
			//
			// For example, when a program throws a page fault, and this page is brought into use,
			// its Modified flag will be kept as one, its reference count will be increased, and it
			// won't be added to the modified page list. If it's fully unmapped again by the time we
			// finish this IO operation, then the page will be re-added to the modified page list.
			//
			// However, even after all of that, if we attempt to call MiTransformPageToStandbyPfn,
			// STILL, nothing will happen, because the page was modified *after* we started the IO
			// process.  At worst, data that is partially written is written to disk for a bit, and
			// then the actual data that was meant to be written, is written.
			
			// Flush the page frame to disk.
			Status = MiFlushModifiedPage(Pfn);
			
			Ipl = MiLockPfdb();
			
			// Remove the reference we added.
			Pfdbe->RefCount--;
			
			if (FAILED(Status))
			{
				// ERROR: At this point writing has failed.  Log it, re-insert it at the end of the
				// queue and move on.
				//
				// TODO: Sleep if the only PFNs are once that failed to be written.  Otherwise, this
				// may end up in an infinite loop that uses 100% of one CPU.
				DbgPrint(
					"MmModifiedPageWriterWorker ERROR: Cannot write PFN %d to backing store. "
					"Failure status: %d (%s)",
					Pfn,
					Status,
					RtlGetStatusString(Status)
				);
				
				MiReinsertIntoModifiedList(Pfn);
			}
			else
			{
				// Mark it as standby.
				MiTransformPageToStandbyPfn(Pfn);
			}
		}
	}
}

INIT
void MmInitializeModifiedPageWriter()
{
	KeInitializeEvent(&MmModifiedPageWriterEvent, EVENT_SYNCHRONIZATION, false);
	
	PETHREAD Thread;
	
	BSTATUS Status = PsCreateSystemThreadFast(
		&Thread,
		MmModifiedPageWriterWorker,
		NULL,
		false
	);
	
	if (FAILED(Status))
	{
		KeCrash(
			"ERROR: Could not launch modified page writer worker: %d (%s)",
			Status,
			RtlGetStatusString(Status)
		);
	}
	
	// TODO: The object manager reaper worker has real time priority.
	// Do we want this worker to have real time priority too?
	
	// NOTE: It really doesn't matter if we do this or not.
	ObDereferenceObject(Thread);
}
