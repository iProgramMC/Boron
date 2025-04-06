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

#define IOSB_STATUS(iosb, stat) (iosb->Status = stat)

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

static BSTATUS IopPerformOperationFileLocked(
	PFILE_OBJECT FileObject,
	PIO_STATUS_BLOCK Iosb,
	int IoType,
	PMDL Mdl,
	uint32_t Flags // See io/dispatch.h for information about this field
)
{
	// The file control block is locked.
	ASSERT(FileObject->Fcb);
	ASSERT(FileObject->Fcb->DispatchTable);
	
	BSTATUS Status;
	
	PFCB Fcb = FileObject->Fcb;
	PIO_DISPATCH_TABLE Dispatch = Fcb->DispatchTable;
	
	switch (IoType)
	{
		case IO_OP_READ:
		{
			IO_READ_METHOD ReadMethod = Dispatch->Read;
			
			if (!ReadMethod)
				return IOSB_STATUS(Iosb, STATUS_UNSUPPORTED_FUNCTION);
			
			Status = ReadMethod (Iosb, Fcb, FileObject->Offset, Mdl, Flags);
			break;
		}
		case IO_OP_WRITE:
		{
			IO_WRITE_METHOD WriteMethod = Dispatch->Write;
			
			if (!WriteMethod)
				return IOSB_STATUS(Iosb, STATUS_UNSUPPORTED_FUNCTION);
			
			Status = WriteMethod (Iosb, Fcb, FileObject->Offset, Mdl, Flags);
			break;
		}
		default:
		{
			Status = STATUS_UNIMPLEMENTED;
			ASSERT(!"IopPerformOperationFileLocked: Unimplemented IoType");
			break;
		}
	}
	
	if (FAILED(Status))
		return Status;
	
	// The read method succeeded, so advance the offset by however many bytes were read.
	FileObject->Offset += Iosb->BytesRead;
	
	return STATUS_SUCCESS;
}

// NOTE: The presence of BufferDst and BufferSrc is mutually exclusive.
//
// On IO_OP_READ, BufferDst is used, and on IO_OP_WRITE, BufferSrc is used.
// This is to keep const correctness throughout the code.
BSTATUS IoPerformOperationFile(
	PIO_STATUS_BLOCK Iosb,
	PFILE_OBJECT FileObject,
	int IoType,
	PMDL Mdl,
	uint32_t Flags
)
{
	// TODO: Cache would intervene around here
	// Currently, this only calls into the device's IO dispatch routines, no caching involved.
	
	ASSERT(FileObject->Fcb);
	ASSERT(FileObject->Fcb->DispatchTable);
	
	BSTATUS Status;
	Flags &= ~IO_RW_LOCKEDEXCLUSIVE;
	
	PFCB Fcb = FileObject->Fcb;
	PIO_DISPATCH_TABLE Dispatch = Fcb->DispatchTable;
	
	// If the DISPATCH_FLAG_EXCLUSIVE flag is set, always lock the FCB's rwlock exclusively.
	//
	// As an optimization, if the file is opened in append only mode, and the operation is
	// a write, then lock the FCB's rwlock exclusive, instead of locking it shared, then
	// unlocking it, and then locking it exclusive.
	if ((Dispatch->Flags & DISPATCH_FLAG_EXCLUSIVE) ||
		(IoType == IO_OP_WRITE && (FileObject->Flags & FILE_FLAG_APPEND_ONLY)))
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
	
	Status = IopPerformOperationFileLocked(FileObject, Iosb, IoType, Mdl, Flags);
	
	IoUnlockFcb(Fcb);
	
	if (Status == STATUS_SUCCESS)
	{
		// NOTE: Dropping status here.  It is not important whether or not the file
		// was touched successfully after the operation was performed on it -- it's
		// not considered important.
		//
		// The reason IopTouchFile returns a status is to implement a call called
		// OSTouchFile which calls this underneath.
		(void) IopTouchFile(Fcb, IoType);
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
	BSTATUS Status;
	
	ASSERT(FileObject->Fcb);
	ASSERT(FileObject->Fcb->DispatchTable);
	
	int Flags = IO_RW_PAGING;
	
	PFCB Fcb = FileObject->Fcb;
	PIO_DISPATCH_TABLE Dispatch = Fcb->DispatchTable;
	
	// If the DISPATCH_FLAG_EXCLUSIVE flag is set, always lock the FCB's rwlock exclusively.
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
	
	// Now perform the actual read operation.
	IO_READ_METHOD ReadMethod = Dispatch->Read;
	
	if (!ReadMethod)
	{
		IoUnlockFcb(Fcb);
		return IOSB_STATUS(Iosb, STATUS_UNSUPPORTED_FUNCTION);
	}
	
	Status = ReadMethod (Iosb, Fcb, FileOffset, Mdl, Flags);
	
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

BSTATUS IoPerformOperationFileHandle(
	PIO_STATUS_BLOCK Iosb, 
	HANDLE Handle,
	int IoType,
	PMDL Mdl,
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
	
	Status = IoPerformOperationFile(Iosb, FileObject, IoType, Mdl, Flags);
	
	ObDereferenceObject(FileObject);
	
	// See the notes in include/io/dispatch.h.
	ASSERT(Iosb->Status == Status);
	
	return Status;
}

// =========== User-facing API ===========

// Note: Calling a sub-function to avoid useless duplication.

BSTATUS IoReadFile(
	PIO_STATUS_BLOCK Iosb,
	HANDLE Handle,
	PMDL Mdl,
	uint32_t Flags
)
{
	return IoPerformOperationFileHandle(Iosb, Handle, IO_OP_READ, Mdl, Flags);
}

BSTATUS IoWriteFile(
	PIO_STATUS_BLOCK Iosb,
	HANDLE Handle,
	PMDL Mdl,
	uint32_t Flags
)
{
	return IoPerformOperationFileHandle(Iosb, Handle, IO_OP_WRITE, Mdl, Flags);
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

BSTATUS OSReadFile(PIO_STATUS_BLOCK Iosb, HANDLE Handle, void* Buffer, size_t Length, uint32_t Flags)
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
	Status = IoReadFile(&Iosb2, Handle, Mdl, Flags);
	
	// This unmaps and unpins the pages, and frees the MDL.
	MmFreeMdl(Mdl);
	
	// Copy the I/O status block to the caller.
	Status = MmSafeCopy(Iosb, &Iosb2, sizeof(IO_STATUS_BLOCK), KeGetPreviousMode(), false);
	if (FAILED(Status))
		return Status;
	
	return Status;
}

BSTATUS OSOpenFile(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes)
{
	return ExOpenObjectUserCall(OutHandle, ObjectAttributes, IoFileType);
}
