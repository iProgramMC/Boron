/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/section.c
	
Abstract:
	This module implements the section object.
	
	The section object represents a section of memory, either backed
	by a file, or backed by virtual memory and page files.
	
Author:
	iProgramInCpp - 7 December 2025
***/
#include "mi.h"

typedef union
{
	struct
	{
		MMPFN Pfn : 32;
	}
	PACKED
	Data;
	
	MMSLA_ENTRY Entry;
}
MMSECTION_SLA_ENTRY, *PMMSECTION_SLA_ENTRY;

static void MmpFreeEntrySectionObject(MMSLA_ENTRY Entry)
{
	// TODO
	(void) Entry;
}

void MmDeleteSectionObject(UNUSED void* ObjectV)
{
	PMMSECTION Section = ObjectV;
	MmDeinitializeSla(&Section->Sla, MmpFreeEntrySectionObject);
}

void MmInitializeSectionObject(PMMSECTION Section)
{
	KeInitializeMutex(&Section->Mutex, 0);
}

BSTATUS MmGetPageSection(void* MappableObject, uint64_t SectionOffset, PMMPFN OutPfn)
{
	PMMSECTION Section = MappableObject;
	
	BSTATUS Status = KeWaitForSingleObject(&Section->Mutex, false, TIMEOUT_INFINITE, MODE_KERNEL);
	ASSERT(SUCCEEDED(Status));
	
	// N.B. Unlike page caches, we don't need to worry about anything modifying the list while
	// it's locked with a mutex.
	MMSECTION_SLA_ENTRY SlaEntry;
	SlaEntry.Entry = MmLookUpEntrySla(&Section->Sla, SectionOffset);
	if (SlaEntry.Entry == MM_SLA_NO_DATA)
	{
		MMPFN Pfn = MmAllocatePhysicalPage();
		if (Pfn == PFN_INVALID)
		{
			KeReleaseMutex(&Section->Mutex);
			return STATUS_INSUFFICIENT_MEMORY;
		}
		
		SlaEntry.Entry = 0;
		SlaEntry.Data.Pfn = Pfn;
		
		MMSLA_ENTRY NewEntry = MmAssignEntrySla(&Section->Sla, SectionOffset, SlaEntry.Entry);
		if (NewEntry == MM_SLA_OUT_OF_MEMORY)
		{
			MmFreePhysicalPage(Pfn);
			KeReleaseMutex(&Section->Mutex);
			return STATUS_INSUFFICIENT_MEMORY;
		}
		
		ASSERT(NewEntry == SlaEntry.Entry);
	}
	
	MmPageAddReference(SlaEntry.Data.Pfn);
	KeReleaseMutex(&Section->Mutex);
	*OutPfn = SlaEntry.Data.Pfn;
	return STATUS_SUCCESS;
}

BSTATUS MmPrepareWriteSection(void* MappableObject, uint64_t Section)
{
	(void) MappableObject;
	(void) Section;
	
	// Sections are shared, so no special programming needed.
	return STATUS_SUCCESS;
}
