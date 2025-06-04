/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/rdwr.c
	
Abstract:
	This module implements the read and write functions
	for the I/O manager.
	
Author:
	iProgramInCpp - 1 July 2024
***/
#include "iop.h"

enum
{
	IO_OP_READ,
	IO_OP_WRITE
};

#define IO_IS_WRITE(op) ((op) == IO_OP_WRITE)

static BSTATUS IopTouchFile(PFCB Fcb, int IoType)
{
	PIO_DISPATCH_TABLE Dispatch = Fcb->DispatchTable;
	
	if (!Dispatch->Touch)
		return STATUS_UNSUPPORTED_FUNCTION;
	
	return Dispatch->Touch(Fcb, IO_IS_WRITE(IoType));
}

static size_t IopGetAlignmentInfo(PFCB Fcb)
{
	PIO_DISPATCH_TABLE Dispatch = Fcb->DispatchTable;
	
	// As noted in include/io/dispatch.h, if the method isn't implemented, a default
	// alignment of 1 is assumed.
	if (!Dispatch->GetAlignmentInfo)
		return 1;
	
	return Dispatch->GetAlignmentInfo(Fcb);
}

static BSTATUS IopReadFileLocked(PIO_STATUS_BLOCK Iosb, PFCB Fcb, PMDL Mdl, uint32_t Flags, uint64_t Offset)
{
	// The file control block is locked.
	ASSERT(Fcb);
	ASSERT(Fcb->DispatchTable);
	
	PIO_DISPATCH_TABLE Dispatch = Fcb->DispatchTable;
	
	IO_READ_METHOD ReadMethod = Dispatch->Read;
	if (!ReadMethod)
		return IOSB_STATUS(Iosb, STATUS_UNSUPPORTED_FUNCTION);
	
	return ReadMethod(Iosb, Fcb, Offset, Mdl, Flags);
}

static BSTATUS IopWriteFileLocked(PIO_STATUS_BLOCK Iosb, PFCB Fcb, PMDL Mdl, uint32_t Flags, uint64_t Offset)
{
	// The file control block is locked.
	ASSERT(Fcb);
	ASSERT(Fcb->DispatchTable);
	
	PIO_DISPATCH_TABLE Dispatch = Fcb->DispatchTable;
	
	IO_WRITE_METHOD WriteMethod = Dispatch->Write;
	if (!WriteMethod)
		return IOSB_STATUS(Iosb, STATUS_UNSUPPORTED_FUNCTION);
	
	return WriteMethod(Iosb, Fcb, Offset, Mdl, Flags);
}

static BSTATUS IopReadFile(PIO_STATUS_BLOCK Iosb, PFCB Fcb, PMDL Mdl, uint32_t Flags, uint64_t Offset, bool Cached)
{
	// TODO: Cache would intervene around here
	// Currently, this only calls into the device's IO dispatch routines, no caching involved.
	(void) Cached;
	
	ASSERT(Fcb);
	ASSERT(Fcb->DispatchTable);
	
	BSTATUS Status;
	Flags &= ~IO_RW_LOCKEDEXCLUSIVE;
	
	PIO_DISPATCH_TABLE Dispatch = Fcb->DispatchTable;
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
		return IOSB_STATUS(Iosb, Status);
	
	Status = IopReadFileLocked(Iosb, Fcb, Mdl, Flags, Offset);
	
	IoUnlockFcb(Fcb);
	
	if (Status == STATUS_SUCCESS)
	{
		// NOTE: Dropping status here.  It is not important whether or not the file
		// was touched successfully after the operation was performed on it -- it's
		// not considered important.
		//
		// The reason IopTouchFile returns a status is to implement a call called
		// OSTouchFile which calls this underneath.
		(void) IopTouchFile(Fcb, IO_OP_READ);
	}
	
	return Status;
}

static BSTATUS IopWriteFile(PIO_STATUS_BLOCK Iosb, PFCB Fcb, PMDL Mdl, uint32_t Flags, uint32_t FileObjectFlags, uint64_t Offset, bool Cached)
{
	// TODO: Cache would intervene around here
	// Currently, this only calls into the device's IO dispatch routines, no caching involved.
	(void) Cached;
	
	ASSERT(Fcb);
	ASSERT(Fcb->DispatchTable);
	
	BSTATUS Status;
	Flags &= ~IO_RW_LOCKEDEXCLUSIVE;
	
	// As an optimization, if the file is opened in append only mode, and the operation is
	// a write, then lock the FCB's rwlock exclusive, instead of locking it shared, then
	// unlocking it, and then locking it exclusive.
	PIO_DISPATCH_TABLE Dispatch = Fcb->DispatchTable;
	if ((Dispatch->Flags & DISPATCH_FLAG_EXCLUSIVE) ||
		(FileObjectFlags & FILE_FLAG_APPEND_ONLY))
	{
		Flags |= IO_RW_LOCKEDEXCLUSIVE;
		Status = IoLockFcbExclusive(Fcb);
	}
	else
	{
		Status = IoLockFcbShared(Fcb);
	}
	
	if (FAILED(Status))
		return IOSB_STATUS(Iosb, Status);
	
	Status = IopWriteFileLocked(Iosb, Fcb, Mdl, Flags, Offset);
	
	IoUnlockFcb(Fcb);
	
	if (Status == STATUS_SUCCESS)
	{
		// NOTE: Dropping status here.  It is not important whether or not the file
		// was touched successfully after the operation was performed on it -- it's
		// not considered important.
		//
		// The reason IopTouchFile returns a status is to implement a call called
		// OSTouchFile which calls this underneath.
		(void) IopTouchFile(Fcb, IO_OP_WRITE);
	}
	
	return Status;
}

BSTATUS IoPerformPagingRead(
	PIO_STATUS_BLOCK Iosb,
	PFILE_OBJECT FileObject,
	PMDL Mdl,
	uint64_t FileOffset
)
{
	ASSERT(FileObject->Fcb);
	
	return IopReadFile(Iosb, FileObject->Fcb, Mdl, IO_RW_PAGING, FileOffset, true);
}

BSTATUS IoPerformOperationFileHandle(
	PIO_STATUS_BLOCK Iosb, 
	HANDLE Handle,
	int IoType,
	PMDL Mdl,
	uint64_t FileOffset,
	uint32_t Flags
)
{
	// NOTE: Parameters are trusted and are assumed to source from kernel mode.
	//
	// For user mode, OSReadFile and OSWriteFile (the system service routines calling
	// IoReadFile and IoWriteFile) will perform the necessary checks to ensure that
	// the operation is valid, and that the buffer and its length are mapped the entire
	// time by capturing the relevant pages using an MDL.
	
	BSTATUS Status;
	
	void* FileObjectV;
	Status = ObReferenceObjectByHandle(Handle, IoFileType, &FileObjectV);
	if (FAILED(Status))
		return IOSB_STATUS(Iosb, Status);
	
	PFILE_OBJECT FileObject = FileObjectV;
	
	if (IoType == IO_OP_READ)
		Status = IopReadFile(Iosb, FileObject->Fcb, Mdl, Flags, FileOffset, true);
	else
		Status = IopWriteFile(Iosb, FileObject->Fcb, Mdl, Flags, FileObject->Flags, FileOffset, true);
	
	ObDereferenceObject(FileObject);
	
	// See the notes in include/io/dispatch.h.
	ASSERT(Iosb->Status == Status);
	
	return Status;
}

// Helpers to read from device or file objects without holding a handle to them.
BSTATUS IoReadDevice(
	PIO_STATUS_BLOCK Iosb,
	PDEVICE_OBJECT DeviceObject,
	PMDL Mdl,
	uint64_t FileOffset,
	bool Cached
)
{
	ASSERT(DeviceObject->Fcb);
	return IopReadFile(Iosb, DeviceObject->Fcb, Mdl, 0, FileOffset, Cached);
}

BSTATUS IoWriteDevice(
	PIO_STATUS_BLOCK Iosb,
	PDEVICE_OBJECT DeviceObject,
	PMDL Mdl,
	uint64_t FileOffset,
	bool Cached
)
{
	ASSERT(DeviceObject->Fcb);
	return IopWriteFile(Iosb, DeviceObject->Fcb, Mdl, 0, 0, FileOffset, Cached);
}

BSTATUS IoReadFileObject(
	PIO_STATUS_BLOCK Iosb,
	PFILE_OBJECT FileObject,
	PMDL Mdl,
	uint64_t FileOffset,
	bool Cached
)
{
	ASSERT(FileObject->Fcb);
	return IopReadFile(Iosb, FileObject->Fcb, Mdl, 0, FileOffset, Cached);
}

BSTATUS IoWriteFileObject(
	PIO_STATUS_BLOCK Iosb,
	PFILE_OBJECT FileObject,
	PMDL Mdl,
	uint64_t FileOffset,
	bool Cached)
{
	ASSERT(FileObject->Fcb);
	return IopWriteFile(Iosb, FileObject->Fcb, Mdl, 0, FileObject->Flags, FileOffset, Cached);
}

// =========== User-facing API ===========

// Note: Calling a sub-function to avoid useless duplication.

BSTATUS IoReadFile(
	PIO_STATUS_BLOCK Iosb,
	HANDLE Handle,
	PMDL Mdl,
	uint64_t FileOffset,
	uint32_t Flags
)
{
	return IoPerformOperationFileHandle(Iosb, Handle, IO_OP_READ, Mdl, FileOffset, Flags);
}

BSTATUS IoWriteFile(
	PIO_STATUS_BLOCK Iosb,
	HANDLE Handle,
	PMDL Mdl,
	uint64_t FileOffset,
	uint32_t Flags)
{
	return IoPerformOperationFileHandle(Iosb, Handle, IO_OP_WRITE, Mdl, FileOffset, Flags);
}

BSTATUS IoTouchFile(HANDLE Handle, bool IsWrite)
{
	BSTATUS Status;
	
	void* FileObjectV;
	Status = ObReferenceObjectByHandle(Handle, IoFileType, &FileObjectV);
	if (FAILED(Status))
		return Status;
	
	PFILE_OBJECT FileObject = FileObjectV;
	
	Status = IopTouchFile(FileObject->Fcb, IsWrite ? IO_OP_WRITE : IO_OP_READ);
	
	ObDereferenceObject(FileObject);
	return Status;
}

BSTATUS IoGetAlignmentInfo(HANDLE Handle, size_t* AlignmentOut)
{
	BSTATUS Status;
	
	void* FileObjectV;
	Status = ObReferenceObjectByHandle(Handle, IoFileType, &FileObjectV);
	if (FAILED(Status))
		return Status;
	
	PFILE_OBJECT FileObject = FileObjectV;
	*AlignmentOut = IopGetAlignmentInfo(FileObject->Fcb);
	
	ObDereferenceObject(FileObject);
	return STATUS_SUCCESS;
}

#define IO_STATUS(Iosb, Stat) ((Iosb)->Status = (Stat))

// TODO: There are several limitations in place:
// - the supporting device driver's max read size
// - the MDL has a max size of like 4MB
//
// Perhaps it's a good idea to restrict IoReadFile/IoWriteFile to like 1MB, and have these issue multiple calls down below.

BSTATUS OSReadFile(PIO_STATUS_BLOCK Iosb, HANDLE Handle, uint64_t ByteOffset, void* Buffer, size_t Length, uint32_t Flags)
{
	BSTATUS Status;
	
	Status = MmProbeAddress(Iosb, sizeof(*Iosb), true, KeGetPreviousMode());
	if (FAILED(Status))
		return Status;
	
	Status = MmProbeAddress(Buffer, Length, true, KeGetPreviousMode());
	if (FAILED(Status))
		return Status;
	
	// Allocate the MDL.
	PMDL Mdl = MmAllocateMdl((uintptr_t) Buffer, Length);
	if (!Mdl)
		return STATUS_INSUFFICIENT_MEMORY;
	
	// Then probe and pin the pages.
	Status = MmProbeAndPinPagesMdl(Mdl, KeGetPreviousMode(), true);
	if (FAILED(Status))
	{
		MmFreeMdl(Mdl);
		return IO_STATUS(Iosb, Status);
	}
	
	// Finally, call the Io function.
	IO_STATUS_BLOCK Iosb2;
	Status = IoReadFile(&Iosb2, Handle, Mdl, ByteOffset, Flags);
	
	// This unmaps and unpins the pages, and frees the MDL.
	MmFreeMdl(Mdl);
	
	// Copy the I/O status block to the caller.
	BSTATUS Status2 = MmSafeCopy(Iosb, &Iosb2, sizeof(IO_STATUS_BLOCK), KeGetPreviousMode(), false);
	
	if (FAILED(Status))
		return Status;
	
	return Status2;
}

BSTATUS OSOpenFile(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes)
{
	return ExOpenObjectUserCall(OutHandle, ObjectAttributes, IoFileType);
}

BSTATUS OSGetLengthFile(HANDLE FileHandle, uint64_t* OutLength)
{
	BSTATUS Status;
	void* FileObjectV;
	
	Status = ObReferenceObjectByHandle(FileHandle, IoFileType, &FileObjectV);
	if (FAILED(Status))
		return Status;
	
	PFILE_OBJECT FileObject = FileObjectV;
	
	if (!IoIsSeekable(FileObject))
	{
		ObDereferenceObject(FileObject);
		return STATUS_UNSUPPORTED_FUNCTION;
	}
	
	uint64_t Length = AtLoad(FileObject->Fcb->FileLength);
	ObDereferenceObject(FileObject);
	
	return MmSafeCopy(OutLength, &Length, sizeof(uint64_t), KeGetPreviousMode(), true);
}
