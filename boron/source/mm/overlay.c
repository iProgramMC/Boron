/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/overlay.c
	
Abstract:
	This module implements the copy-on-write overlay object.
	
	The copy-on-write overlay object is designed to plug over
	an existing mappable object (section, overlay, or file)
	and implement copy-on-write functionality while also
	facilitating page-out mechanics.
	
	The object stores *only* the differences between the current
	written-private state and the original read-shared state.
	
	The Boron kernel allows private (copy-on-write) mappings to
	be modified while they are in the read state. This is not an
	oversight, a bug, or a problem. Major kernels also do this.
	The alternative would be eagerly cloning shared state into
	private mappings which defeats the whole purpose of copy-on-
	write. Also, the only place this matters is in file backed
	mappings; anonymous private sections are duplicated on both
	sides (known as "symmetric CoW")
	
Author:
	iProgramInCpp - 7 December 2025
***/
#include "mi.h"

#ifdef IS_32_BIT

// TODO: optimize this!
static uint8_t MmpCowOverlayBuffer[PAGE_SIZE];

#endif

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
MMOVERLAY_SLA_ENTRY, *PMMOVERLAY_SLA_ENTRY;

static void MmpFreeEntryOverlayObject(MMSLA_ENTRY Entry)
{
	MMOVERLAY_SLA_ENTRY OverlayEntry;
	OverlayEntry.Entry = Entry;
	
	MMPFN Pfn = OverlayEntry.Data.Pfn;
	if (Pfn != PFN_INVALID)
		MmFreePhysicalPage(Pfn);
}

static BSTATUS MmpGetPageOverlay(void* MappableObject, uint64_t SectionOffset, PMMPFN OutPfn)
{
	PMMOVERLAY Overlay = MappableObject;
	SectionOffset += Overlay->SectionOffset;
	
	BSTATUS Status = KeWaitForSingleObject(&Overlay->Mutex, false, TIMEOUT_INFINITE, MODE_KERNEL);
	ASSERT(SUCCEEDED(Status));
	
	// N.B. Unlike page caches, we don't need to worry about anything modifying the list while
	// it's locked with a mutex.
	MMOVERLAY_SLA_ENTRY SlaEntry;
	SlaEntry.Entry = MmLookUpEntrySla(&Overlay->Sla, SectionOffset);
	if (SlaEntry.Entry != MM_SLA_NO_DATA)
	{
		// The CoW overlay has its own page here, so return that.
		MmPageAddReference(SlaEntry.Data.Pfn);
		KeReleaseMutex(&Overlay->Mutex);
		*OutPfn = SlaEntry.Data.Pfn;
		return STATUS_SUCCESS;
	}
	
	KeReleaseMutex(&Overlay->Mutex);
	return MmGetPageMappable(Overlay->Parent, SectionOffset, OutPfn);
}

static BSTATUS MmpReadPageOverlay(void* MappableObject, uint64_t SectionOffset, PMMPFN OutPfn)
{
	PMMOVERLAY Overlay = MappableObject;
	SectionOffset += Overlay->SectionOffset;
	return MmReadPageMappable(Overlay->Parent, SectionOffset, OutPfn);
}

static BSTATUS MmpPrepareWriteOverlay(void* MappableObject, uint64_t SectionOffset)
{
	PMMOVERLAY Overlay = MappableObject;
	SectionOffset += Overlay->SectionOffset;
	
	BSTATUS Status = KeWaitForSingleObject(&Overlay->Mutex, false, TIMEOUT_INFINITE, MODE_KERNEL);
	ASSERT(SUCCEEDED(Status));
	
	MMOVERLAY_SLA_ENTRY SlaEntry;
	SlaEntry.Entry = MmLookUpEntrySla(&Overlay->Sla, SectionOffset);
	if (SlaEntry.Entry != MM_SLA_NO_DATA)
	{
		// There is already data here, so just succeed.
		KeReleaseMutex(&Overlay->Mutex);
		return STATUS_SUCCESS;
	}
	
	// There is no data here.  We need to fetch the page from the parent.
	KeReleaseMutex(&Overlay->Mutex);
	
	MMPFN Pfn = PFN_INVALID;
	Status = MmGetPageMappable(Overlay->Parent, SectionOffset, &Pfn);
	if (Status == STATUS_HARDWARE_IO_ERROR)
	{
		// Need to call ReadPage.  This will return the page after being
		// read into memory.
		Status = MmReadPageMappable(Overlay->Parent, SectionOffset, &Pfn);
	}
	
	if (FAILED(Status))
	{	
		DbgPrint(
			"MmpPrepareWriteOverlay: MmGetPageMappable(%p, %llu) failed! %s",
			Overlay->Parent,
			SectionOffset,
			RtlGetStatusString(Status)
		);
		return Status;
	}
	
	Status = KeWaitForSingleObject(&Overlay->Mutex, false, TIMEOUT_INFINITE, MODE_KERNEL);
	ASSERT(SUCCEEDED(Status));
	
	SlaEntry.Entry = MmLookUpEntrySla(&Overlay->Sla, SectionOffset);
	if (SlaEntry.Entry != MM_SLA_NO_DATA)
	{
		// There is already data here, so just succeed.  There was another call to
		// this function with the exact same offset that did this too.
		MmFreePhysicalPage(Pfn);
		KeReleaseMutex(&Overlay->Mutex);
		return STATUS_SUCCESS;
	}
	
	// We still need a new PFN, so allocate it here.
	MMPFN NewPfn = MmAllocatePhysicalPage();
	if (NewPfn == PFN_INVALID)
	{
		MmFreePhysicalPage(Pfn);
		KeReleaseMutex(&Overlay->Mutex);
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	// Copy all the data over.
#ifdef IS_64_BIT
	// On 64 bit we can do this directly
	memcpy(
		MmGetHHDMOffsetAddr(MmPFNToPhysPage(NewPfn)),
		MmGetHHDMOffsetAddr(MmPFNToPhysPage(Pfn)),
		PAGE_SIZE
	);
#else
	// On 32-bit this is a bit more complicated, needs optimization.
	MmBeginUsingHHDM();
	
	void* OldMem = MmGetHHDMOffsetAddr(MmPFNToPhysPage(Pfn));
	memcpy(MmpCowOverlayBuffer, OldMem, PAGE_SIZE);
	
	void* NewMem = MmGetHHDMOffsetAddr(MmPFNToPhysPage(NewPfn));
	memcpy(NewMem, MmpCowOverlayBuffer, PAGE_SIZE);
	
	MmEndUsingHHDM();
#endif
	
	MmFreePhysicalPage(Pfn);
	
	// No data here, so assign it now.
	SlaEntry.Entry = 0;
	SlaEntry.Data.Pfn = NewPfn;
	MMSLA_ENTRY NewEntry = MmAssignEntrySla(&Overlay->Sla, SectionOffset, SlaEntry.Entry);
	if (NewEntry == MM_SLA_OUT_OF_MEMORY)
	{
		MmFreePhysicalPage(NewPfn);
		KeReleaseMutex(&Overlay->Mutex);
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	ASSERT(NewEntry == SlaEntry.Entry);
	KeReleaseMutex(&Overlay->Mutex);
	return STATUS_SUCCESS;
}

static MAPPABLE_DISPATCH_TABLE MmpOverlayObjectMappableDispatch =
{
	.GetPage = MmpGetPageOverlay,
	.ReadPage = MmpReadPageOverlay,
	.PrepareWrite = MmpPrepareWriteOverlay
};

void MmDeleteOverlayObject(UNUSED void* ObjectV)
{
	PMMOVERLAY Overlay = ObjectV;
	MmDeinitializeSla(&Overlay->Sla, MmpFreeEntryOverlayObject);
	ObDereferenceObject(Overlay->Parent);
}

void MmInitializeOverlayObject(PMMOVERLAY Overlay, void* ParentMappable, uint64_t SectionOffset)
{
	MmInitializeMappableHeader(&Overlay->Mappable, &MmpOverlayObjectMappableDispatch);
	KeInitializeMutex(&Overlay->Mutex, 4);
	MmInitializeSla(&Overlay->Sla);
	ObReferenceObjectByPointer(ParentMappable);
	Overlay->Parent = ParentMappable;
	Overlay->SectionOffset = SectionOffset;
}

// NOTE: Does NOT reference the new object! You must do that yourself.
// However, if you have the VAD lock, this is safe to do.
//
// NOTE: Does nothing if you don't pass an overlay, just returns the original
// pointer. However, it still checks if the object is mappable!
BSTATUS MiResolveBackingStoreForOverlay(void* Object, void** OutFileOrSectionObject)
{
	MmVerifyMappableHeader(Object);
	
	while (ObGetObjectType(Object) == MmOverlayObjectType)
	{
		PMMOVERLAY Overlay = Object;
		Object = Overlay->Parent;
	}
	
	*OutFileOrSectionObject = Object;
	return STATUS_SUCCESS;
}

BSTATUS MmCreateOverlayObject(
	PMMOVERLAY* OutOverlay,
	void* ParentMappable,
	uint64_t SectionOffset
)
{
	ASSERT(ParentMappable != NULL && "CoW Overlay objects cannot function without a parent.");
	
	MmVerifyMappableHeader(ParentMappable);
	
	void* OutObject;
	BSTATUS Status;
	
	Status = ObCreateObject(
		&OutObject,
		NULL, // ParentDirectory
		MmOverlayObjectType,
		NULL, // ObjectName
		OB_FLAG_NO_DIRECTORY,
		NULL, // ParseContext
		sizeof(MMOVERLAY)
	);
	
	if (FAILED(Status))
		return Status;
	
	PMMOVERLAY Overlay = OutObject;
	MmInitializeOverlayObject(Overlay, ParentMappable, SectionOffset);
	
	*OutOverlay = Overlay;
	return STATUS_SUCCESS;
}
