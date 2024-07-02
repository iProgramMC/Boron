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

#define IOSB_STATUS(iosb, stat) (iosb->Status = stat)

static BSTATUS IopPerformOperationFileLocked(
	PFILE_OBJECT FileObject,
	PIO_STATUS_BLOCK Iosb,
	int IoType,
	void* BufferDst,
	const void* BufferSrc,
	size_t Length,
	bool MayBlock
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
			
			Status = ReadMethod (Iosb, Fcb, FileObject->Offset, Length, BufferDst, MayBlock);
			break;
		}
		case IO_OP_WRITE:
		{
			IO_WRITE_METHOD WriteMethod = Dispatch->Write;
			
			if (!WriteMethod)
				return IOSB_STATUS(Iosb, STATUS_UNSUPPORTED_FUNCTION);
			
			Status = WriteMethod (Iosb, Fcb, FileObject->Offset, Length, BufferSrc, MayBlock);
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
	PFILE_OBJECT FileObject,
	PIO_STATUS_BLOCK Iosb,
	int IoType,
	void* BufferDst,
	const void* BufferSrc,
	size_t Length,
	bool MayBlock
)
{
	// TODO: Cache would intervene around here
	// Currently, this only calls into the device's IO dispatch routines, no caching involved.
	
	ASSERT(FileObject->Fcb);
	ASSERT(FileObject->Fcb->DispatchTable);
	
	BSTATUS Status;
	
	PFCB Fcb = FileObject->Fcb;
	PIO_DISPATCH_TABLE Dispatch = Fcb->DispatchTable;
	
	// With read-only operations, if the dispatch table doesn't say otherwise, lock shared.
	if (IoType == IO_OP_READ && ~(Dispatch->Flags & DISPATCH_FLAG_EXCLUSIVE))
		Status = IoLockFcbShared(Fcb);
	else
		Status = IoLockFcbExclusive(Fcb);
	
	if (FAILED(Status))
		return IOSB_STATUS(Iosb, Status);
	
	Status = IopPerformOperationFileLocked(FileObject, Iosb, IoType, BufferDst, BufferSrc, Length, MayBlock);
	
	IoUnlockFcb(Fcb);
	return Status;
}

BSTATUS IoReadFile(
	PIO_STATUS_BLOCK Iosb,
	HANDLE Handle,
	void* Buffer,
	size_t Length,
	bool CanBlock
)
{
	// NOTE: Parameters are trusted and are assumed to source from kernel mode.
	//
	// For user mode, OSReadFile and OSWriteFile will perform the necessary checks to ensure
	// that the operation is valid, and that the buffer and its length are mapped the entire
	// time by capturing the relevant pages using an MDL.
	
	BSTATUS Status;
	
	// Resolve the handle.
	void* FileObjectV;
	Status = ObReferenceObjectByHandle(Handle, &FileObjectV);
	if (FAILED(Status))
		return IOSB_STATUS(Iosb, Status);
	
	PFILE_OBJECT FileObject = FileObjectV;
	
	Status = IoPerformOperationFile(FileObject, Iosb, IO_OP_READ, Buffer, NULL, Length, CanBlock);
	
	ObDereferenceObject(FileObject);
	
	// See the notes in include/io/dispatch.h.
	ASSERT(Iosb->Status == Status);
	
	return Status;
}

BSTATUS IoWriteFile(
	PIO_STATUS_BLOCK Iosb,
	HANDLE Handle,
	const void* Buffer,
	size_t Length,
	bool CanBlock
)
{
	BSTATUS Status;
	
	// Resolve the handle.
	void* FileObjectV;
	Status = ObReferenceObjectByHandle(Handle, &FileObjectV);
	if (FAILED(Status))
		return IOSB_STATUS(Iosb, Status);
	
	PFILE_OBJECT FileObject = FileObjectV;
	
	Status = IoPerformOperationFile(FileObject, Iosb, IO_OP_WRITE, NULL, Buffer, Length, CanBlock);
	
	ObDereferenceObject(FileObject);
	
	// See the notes in include/io/dispatch.h.
	ASSERT(Iosb->Status == Status);
	
	return Status;
}
