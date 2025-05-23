/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	main.c
	
Abstract:
	This module implements the partition manager component
	of the Boron kernel.
	
Author:
	iProgramInCpp - 30 April 2025
***/
#include "iop.h"

// The list of partitionable devices.
static LIST_ENTRY IopPartitionableListHead;
static KMUTEX IopPartitionableListLock;

// The list of identifiable file systems.
static LIST_ENTRY IopFileSystemListHead;
static KMUTEX IopFileSystemListLock;

// Registers a device as partitionable.  When driver initialization
// is finished, the partition manager will scan every registered
// device for partitions.
//
// MBR will be implemented first, and then GPT.
void IoRegisterPartitionableDevice(PDEVICE_OBJECT DeviceObject)
{
	KeWaitForSingleObject(&IopPartitionableListLock, false, TIMEOUT_INFINITE, MODE_KERNEL);
	InsertTailList(&IopPartitionableListHead, &DeviceObject->PartitionableListEntry);
	KeReleaseMutex(&IopPartitionableListLock);
	ObReferenceObjectByPointer(DeviceObject);
}

// Registers a file system that could be found using the partition manager.
void IoRegisterFileSystemDriver(PIO_DISPATCH_TABLE DispatchTable)
{
	KeWaitForSingleObject(&IopFileSystemListLock, false, TIMEOUT_INFINITE, MODE_KERNEL);
	InsertTailList(&IopFileSystemListHead, &DispatchTable->FileSystemListEntry);
	KeReleaseMutex(&IopFileSystemListLock);
}

void IopInitPartitionManager()
{
	IopInitializePartitionDriverObject();
	KeInitializeMutex(&IopPartitionableListLock, 0);
	KeInitializeMutex(&IopFileSystemListLock, 0);
	InitializeListHead(&IopPartitionableListHead);
	InitializeListHead(&IopFileSystemListHead);
}

BSTATUS IopReadFromDeviceObject(PDEVICE_OBJECT DeviceObject, uint8_t* Data, size_t Size, uint64_t Offset)
{
	MMPFN TempPage = MmAllocatePhysicalPage();
	if (TempPage == PFN_INVALID)
		return STATUS_INSUFFICIENT_MEMORY;
	
	UNUSED BSTATUS Status = STATUS_SUCCESS;
	
	MDL_ONEPAGE Mdl;
	MmInitializeSinglePageMdl(&Mdl, TempPage, MDL_FLAG_WRITE);
	
	PFCB Fcb = DeviceObject->Fcb;
	PIO_DISPATCH_TABLE Dispatch = DeviceObject->DispatchTable;
	
	ASSERT(Fcb);
	ASSERT(Dispatch);
	
	uint32_t Flags = 0;
	
	if (Dispatch->Flags & DISPATCH_FLAG_EXCLUSIVE)
	{
		Flags |= IO_RW_LOCKEDEXCLUSIVE;
		Status = IoLockFcbExclusive(Fcb);
	}
	else
	{
		Status = IoLockFcbShared(Fcb);
	}
	
	if (FAILED(Status))
		goto Done2;
	
	IO_READ_METHOD ReadMethod = Dispatch->Read;
	if (!ReadMethod)
	{
		Status = STATUS_UNSUPPORTED_FUNCTION;
		goto Done;
	}
	
	IO_STATUS_BLOCK Iosb;
	memset(&Iosb, 0, sizeof Iosb);
	
	Status = ReadMethod(&Iosb, Fcb, Offset, &Mdl.Base, Flags);
	
	if (SUCCEEDED(Status))
		memcpy(Data, MmGetHHDMOffsetAddr(MmPFNToPhysPage(TempPage)), Size);
	
Done:
	IoUnlockFcb(Fcb);
Done2:
	MmFreeMdl(&Mdl.Base);
	MmFreePhysicalPage(TempPage);
	return Status;
}

static void IopRegisterMbrPartition(PDEVICE_OBJECT Device, PMBR_PARTITION MbrPartition, int Number)
{
	// Create the partition object.
	PDEVICE_OBJECT Object = NULL;
	BSTATUS Status = IoCreatePartition(
		&Object,
		Device,
		MbrPartition->StartLBA * 512,
		MbrPartition->PartSizeSectors * 512,
		Number
	);
	
	if (FAILED(Status))
		KeCrash("Cannot create partition object: %d (%s).", Status, RtlGetStatusString(Status)); // TODO: More comprehensive debug info.
	
	// Check the registerable file systems.
	PLIST_ENTRY Entry = IopFileSystemListHead.Flink;
	while (Entry != &IopFileSystemListHead)
	{
		PIO_DISPATCH_TABLE Dispatch = CONTAINING_RECORD(Entry, IO_DISPATCH_TABLE, FileSystemListEntry);
		
		ASSERT(Dispatch->Mount);
		
		BSTATUS Status = Dispatch->Mount(Object);
		if (FAILED(Status) && Status != STATUS_NOT_THIS_FILE_SYSTEM)
			KeCrash("Failed to mount partition %d: %d (%s)", Number, Status, RtlGetStatusString(Status));
		
		Entry = Entry->Flink;
	}
}

static void IopScanForFileSystemsOnDevice(PDEVICE_OBJECT Device)
{
	BSTATUS Status;
#ifdef DEBUG
	const char* Name = OBJECT_GET_HEADER(Device)->ObjectName;
	LogMsg("Scanning %s for partitions...", Name);
#endif
	
	// Try MBR first
	uint8_t Header[512];
	
	Status = IopReadFromDeviceObject(Device, Header, sizeof Header, 0);
	if (FAILED(Status))
		goto Done;
	
	// See if this is indeed an MBR
	PMASTER_BOOT_RECORD Mbr = (void*) Header;
	
	if (Mbr->BootSignature != MBR_BOOT_SIGNATURE)
	{
		Status = STATUS_INVALID_HEADER;
		goto Done;
	}
	
	// Scan the partition list
	for (int i = 0; i < 4; i++)
	{
		PMBR_PARTITION Partition = &Mbr->Partitions[i];
		
		if (Partition->PartTypeDesc == 0)
			// If this entry is unused.
			continue;
		
		if (Partition->StartLBA == 0 || Partition->PartSizeSectors == 0)
			// If this entry has invalid LBA settings.  We do not support CHS addressing.
			continue;
		
		IopRegisterMbrPartition(Device, Partition, i);
	}
	
	// Create partition objects
	
	// Scan for file systems on those
	
	// Etc...
	
Done:
	if (FAILED(Status))
		DbgPrint("Failed to find partitions on %s: %d (%s)", Name, Status, RtlGetStatusString(Status));
}

// Initiates the file system scanning procedure.
void IoScanForFileSystems()
{
	void* Objects[] = { &IopPartitionableListLock, &IopFileSystemListLock };
	KeWaitForMultipleObjects(2, Objects, WAIT_TYPE_ALL, false, TIMEOUT_INFINITE, NULL, MODE_KERNEL);
	
	for (PLIST_ENTRY Entry = IopPartitionableListHead.Flink;
		Entry != &IopPartitionableListHead;
		Entry = Entry->Flink)
	{
		PDEVICE_OBJECT Device = CONTAINING_RECORD(Entry, DEVICE_OBJECT, PartitionableListEntry);
		
		IopScanForFileSystemsOnDevice(Device);
	}
	
	KeReleaseMutex(&IopPartitionableListLock);
	KeReleaseMutex(&IopFileSystemListLock);
}
