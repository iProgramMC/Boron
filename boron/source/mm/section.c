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
#include <ex.h>

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
	MMSECTION_SLA_ENTRY SectionEntry;
	SectionEntry.Entry = Entry;
	
	MMPFN Pfn = SectionEntry.Data.Pfn;
	if (Pfn != PFN_INVALID)
		MmFreePhysicalPage(Pfn);
}

static BSTATUS MmpGetPageSection(void* MappableObject, uint64_t SectionOffset, PMMPFN OutPfn)
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

static BSTATUS MmpReadPageSection(void* MappableObject, uint64_t SectionOffset, PMMPFN OutPfn)
{
	(void) MappableObject;
	(void) SectionOffset;
	(void) OutPfn;
	
	// There is nothing to page in. (for now!)
	return STATUS_HARDWARE_IO_ERROR;
}

static BSTATUS MmpPrepareWriteSection(void* MappableObject, uint64_t SectionOffset)
{
	(void) MappableObject;
	(void) SectionOffset;
	
	// Sections are shared, so no special programming needed.
	return STATUS_SUCCESS;
}

static MAPPABLE_DISPATCH_TABLE MmpSectionObjectMappableDispatch =
{
	.GetPage = MmpGetPageSection,
	.ReadPage = MmpReadPageSection,
	.PrepareWrite = MmpPrepareWriteSection
};

void MmDeleteSectionObject(UNUSED void* ObjectV)
{
	PMMSECTION Section = ObjectV;
	MmDeinitializeSla(&Section->Sla, MmpFreeEntrySectionObject);
}

void MmInitializeSectionObject(PMMSECTION Section)
{
	MmInitializeMappableHeader(&Section->Mappable, &MmpSectionObjectMappableDispatch);
	KeInitializeMutex(&Section->Mutex, 4);
	MmInitializeSla(&Section->Sla);
	Section->MaxSizePages = 0;
}

typedef struct
{
	uint64_t MaxSize;
}
SECTION_CREATE_CONTEXT;

static BSTATUS MmpInitializeSectionObject(void* Object, void* Context)
{
	PMMSECTION Section = Object;
	SECTION_CREATE_CONTEXT* CreateContext = Context;
	
	MmInitializeSectionObject(Section);
	Section->MaxSizePages = (CreateContext->MaxSize + PAGE_SIZE - 1) / PAGE_SIZE;
	
	return STATUS_SUCCESS;
}

BSTATUS MmCreateAnonymousSectionObject(PMMSECTION* OutSection, uint64_t MaxSize)
{
	void* OutObject;
	BSTATUS Status;
	
	Status = ObCreateObject(
		&OutObject,
		NULL, // ParentDirectory
		MmSectionObjectType,
		NULL, // ObjectName
		OB_FLAG_NO_DIRECTORY,
		NULL, // ParseContext
		sizeof(MMSECTION)
	);
	
	if (FAILED(Status))
		return Status;
	
	SECTION_CREATE_CONTEXT CreateContext;
	CreateContext.MaxSize = MaxSize;
	
	Status = MmpInitializeSectionObject(OutObject, &CreateContext);
	ASSERT(SUCCEEDED(Status));
	
	*OutSection = OutObject;
	return STATUS_SUCCESS;
}

BSTATUS OSCreateSectionObject(
	PHANDLE OutSectionHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	uint64_t MaxSize
)
{
	SECTION_CREATE_CONTEXT CreateContext;
	CreateContext.MaxSize = MaxSize;
	
	return ExCreateObjectUserCall(
		OutSectionHandle,
		ObjectAttributes,
		MmSectionObjectType,
		sizeof(MMSECTION),
		MmpInitializeSectionObject,
		&CreateContext,
		0,
		false
	);
}
