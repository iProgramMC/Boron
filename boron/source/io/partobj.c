/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	main.c
	
Abstract:
	This module implements the partition object
	of the Boron kernel.
	
Author:
	iProgramInCpp - 2 May 2025
***/
#include "iop.h"

typedef struct
{
	uint64_t Offset;
	uint64_t Size;
	PFCB Device;
}
FCB_PART_EXT, *PFCB_PART_EXT;

static void IopPartitionCreateObject(PFCB Fcb, PFILE_OBJECT FileObject)
{
	ASSERT(Fcb->ExtensionSize == sizeof(FCB_PART_EXT));
	PFCB_PART_EXT PartExt = (void*) Fcb->Extension;
	
	if (PartExt->Device->DispatchTable->CreateObject)
		PartExt->Device->DispatchTable->CreateObject(PartExt->Device, FileObject);
}

static void IopPartitionDeleteObject(PFCB Fcb, PFILE_OBJECT FileObject)
{
	ASSERT(Fcb->ExtensionSize == sizeof(FCB_PART_EXT));
	PFCB_PART_EXT PartExt = (void*) Fcb->Extension;
	
	if (PartExt->Device->DispatchTable->DeleteObject)
		PartExt->Device->DispatchTable->DeleteObject(PartExt->Device, FileObject);
}

static BSTATUS IopPartitionRead(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, PMDL MdlBuffer, uint32_t Flags)
{
	BSTATUS Status;
	PFCB_PART_EXT PartExt;
	PFCB Device;
	
	// Assert that this MDL was created as read-write. (we want it to be used as write-only in this case)
	ASSERT(MdlBuffer->Flags & MDL_FLAG_WRITE);
	
	// Assert that this FCB is a partition FCB.
	ASSERT(Fcb->ExtensionSize == sizeof(FCB_PART_EXT));
	
	PartExt = (void*) Fcb->Extension;
	
	// Pass it down to the host FCB.
	Device = PartExt->Device;
	
	// TODO: Make it so out of bounds reads/writes are capped.
	
	if (!Device->DispatchTable->Read)
		return IOSB_STATUS(Iosb, STATUS_UNSUPPORTED_FUNCTION);
	
	if (Device->DispatchTable->Flags & DISPATCH_FLAG_EXCLUSIVE)
	{
		Flags |= IO_RW_LOCKEDEXCLUSIVE;
		Status = IoLockFcbExclusive(Device);
	}
	else
	{
		Status = IoLockFcbShared(Device);
	}
	
	if (FAILED(Status))
		return IOSB_STATUS(Iosb, Status);

	Status = Device->DispatchTable->Read(Iosb, Device, PartExt->Offset + Offset, MdlBuffer, Flags);
	
	IoUnlockFcb(Device);
	
	return Status;
}

static BSTATUS IopPartitionWrite(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, PMDL MdlBuffer, uint32_t Flags)
{
	BSTATUS Status;
	PFCB_PART_EXT PartExt;
	PFCB Device;
	
	// Assert that this MDL was created as read only.
	ASSERT(~MdlBuffer->Flags & MDL_FLAG_WRITE);
	
	// Assert that this FCB is a partition FCB.
	ASSERT(Fcb->ExtensionSize == sizeof(FCB_PART_EXT));
	
	PartExt = (void*) Fcb->Extension;
	
	// Pass it down to the host FCB.
	Device = PartExt->Device;
	
	// TODO: Make it so out of bounds reads/writes are capped.
	
	if (!Device->DispatchTable->Write)
		return IOSB_STATUS(Iosb, STATUS_UNSUPPORTED_FUNCTION);
	
	if (Device->DispatchTable->Flags & DISPATCH_FLAG_EXCLUSIVE)
	{
		Flags |= IO_RW_LOCKEDEXCLUSIVE;
		Status = IoLockFcbExclusive(Device);
	}
	else
	{
		Status = IoLockFcbShared(Device);
	}
	
	if (FAILED(Status))
		return IOSB_STATUS(Iosb, Status);

	Status = Device->DispatchTable->Write(Iosb, Device, PartExt->Offset + Offset, MdlBuffer, Flags);
	
	IoUnlockFcb(Device);
	
	return Status;
}

bool IopPartitionSeekable(UNUSED PFCB Fcb)
{
	// The host FCB had better be seekable!
	return true;
}

static IO_DISPATCH_TABLE IopPartitionDispatchTable = {
	.Flags = 0,
	.CreateObject = IopPartitionCreateObject,
	.DeleteObject = IopPartitionDeleteObject,
	.Read = IopPartitionRead,
	.Write = IopPartitionWrite,
	.Seekable = IopPartitionSeekable
};

static struct {
	NONPAGED_OBJECT_HEADER NPHeader;
	OBJECT_HEADER Header;
	DRIVER_OBJECT Object;
}
IopPartitionDriverObject;

void IopInitializePartitionDriverObject()
{
	// This only sets up the fields necessary for IoCreateDevice to work.
	PNONPAGED_OBJECT_HEADER NPHeader = &IopPartitionDriverObject.NPHeader;
	POBJECT_HEADER Header = &IopPartitionDriverObject.Header;
	PDRIVER_OBJECT Object = &IopPartitionDriverObject.Object;
	
	// Set up the header so that this object is permanent.
	Header->NonPagedObjectHeader = NPHeader;
	Header->Flags = OB_FLAG_KERNEL | OB_FLAG_PERMANENT | OB_FLAG_NO_DIRECTORY | OB_FLAG_NONPAGED;
	Header->BodySize = sizeof(DRIVER_OBJECT);
	
#ifdef OBJECT_HEADER_SIGNATURE
	Header->Signature = OBJECT_HEADER_SIGNATURE;
#endif
	
	// Set up the actual driver object.
	Object->DriverName = "PartitionManager";
	
	InitializeListHead(&Object->DeviceList);
	KeInitializeSpinLock(&Object->DeviceListLock);
}

BSTATUS IoCreatePartition(PDEVICE_OBJECT* OutDevice, PDEVICE_OBJECT InDevice, uint64_t Offset, uint64_t Size, size_t Number)
{
	// Get the name of the object.
	const char* Name = OBJECT_GET_HEADER(InDevice)->ObjectName;
	size_t NameSize = 40; // "UnknownDevice%p"
	if (Name)
		NameSize = strlen(Name);
	
	// Reserve some extra space for the number.
	NameSize += 24;
	
	char* NewName = MmAllocatePool(POOL_PAGED, NameSize);
	if (!NewName)
		return STATUS_INSUFFICIENT_MEMORY;
	
	if (Name)
		strcpy(NewName, Name);
	else
		snprintf(NewName, NameSize, "UnknownDevice_%p_", InDevice);
	
	// Now, append the number to this device.
	sprintf(NewName + strlen(NewName), "Part%zu", Number);
	
	// Create the device object with this name now.
	PDEVICE_OBJECT DeviceObject = NULL;
	BSTATUS Status = IoCreateDevice(
		&IopPartitionDriverObject.Object,
		0,
		sizeof(FCB_PART_EXT),
		NewName,
		DEVICE_TYPE_BLOCK,
		true, // TODO: come back to this
		&IopPartitionDispatchTable,
		&DeviceObject
	);
	
	MmFreePool(NewName);
	
	if (SUCCEEDED(Status))
	{
		*OutDevice = DeviceObject;
		
		PFCB_PART_EXT Ext = (void*) DeviceObject->Fcb->Extension;
		Ext->Offset = Offset;
		Ext->Size = Size;
		Ext->Device = InDevice->Fcb;
	}
	
	return Status;
}
