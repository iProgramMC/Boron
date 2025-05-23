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

static BSTATUS IopPerformOperationFileLocked(
	PFCB Fcb,
	PIO_STATUS_BLOCK Iosb,
	int IoType,
	PMDL Mdl,
	uint32_t Flags, // See io/dispatch.h for information about this field
	uint64_t* Offset
)
{
	// The file control block is locked.
	ASSERT(Fcb);
	ASSERT(Fcb->DispatchTable);
	
	BSTATUS Status;
	
	PIO_DISPATCH_TABLE Dispatch = Fcb->DispatchTable;
	
	switch (IoType)
	{
		case IO_OP_READ:
		{
			IO_READ_METHOD ReadMethod = Dispatch->Read;
			
			if (!ReadMethod)
				return IOSB_STATUS(Iosb, STATUS_UNSUPPORTED_FUNCTION);
			
			Status = ReadMethod (Iosb, Fcb, *Offset, Mdl, Flags);
			break;
		}
		case IO_OP_WRITE:
		{
			IO_WRITE_METHOD WriteMethod = Dispatch->Write;
			
			if (!WriteMethod)
				return IOSB_STATUS(Iosb, STATUS_UNSUPPORTED_FUNCTION);
			
			Status = WriteMethod (Iosb, Fcb, *Offset, Mdl, Flags);
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
	
	// The read/write methods succeeded, so advance the offset by however many bytes were read.
	AtAddFetch(*Offset, Iosb->BytesRead);
	return STATUS_SUCCESS;
}

// NOTE: The presence of BufferDst and BufferSrc is mutually exclusive.
//
// On IO_OP_READ, BufferDst is used, and on IO_OP_WRITE, BufferSrc is used.
// This is to keep const correctness throughout the code.
BSTATUS IoPerformOperationFile(
	PIO_STATUS_BLOCK Iosb,
	PFCB Fcb,
	int IoType,
	PMDL Mdl,
	uint32_t Flags,
	uint32_t FileObjectFlags,
	uint64_t* Offset,
	bool Cached
)
{
	// TODO: Cache would intervene around here
	// Currently, this only calls into the device's IO dispatch routines, no caching involved.
	(void) Cached;
	
	ASSERT(Fcb);
	ASSERT(Fcb->DispatchTable);
	
	BSTATUS Status;
	Flags &= ~IO_RW_LOCKEDEXCLUSIVE;
	
	PIO_DISPATCH_TABLE Dispatch = Fcb->DispatchTable;
	
	// If the DISPATCH_FLAG_EXCLUSIVE flag is set, always lock the FCB's rwlock exclusively.
	//
	// As an optimization, if the file is opened in append only mode, and the operation is
	// a write, then lock the FCB's rwlock exclusive, instead of locking it shared, then
	// unlocking it, and then locking it exclusive.
	if ((Dispatch->Flags & DISPATCH_FLAG_EXCLUSIVE) ||
		(IoType == IO_OP_WRITE && (FileObjectFlags & FILE_FLAG_APPEND_ONLY)))
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
	
	Status = IopPerformOperationFileLocked(Fcb, Iosb, IoType, Mdl, Flags, Offset);
	
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
	ASSERT(FileObject->Fcb);
	return IoPerformOperationFile(Iosb, FileObject->Fcb, IO_OP_READ, Mdl, IO_RW_PAGING, FileObject->Flags, &FileOffset, true);
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
	
	Status = IoPerformOperationFile(Iosb, FileObject->Fcb, IoType, Mdl, Flags, FileObject->Flags, &FileObject->Offset, true);
	
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
	return IoPerformOperationFile(Iosb, DeviceObject->Fcb, IO_OP_READ, Mdl, 0, 0, &FileOffset, Cached);
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
	return IoPerformOperationFile(Iosb, DeviceObject->Fcb, IO_OP_WRITE, Mdl, 0, 0, &FileOffset, Cached);
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
	return IoPerformOperationFile(Iosb, FileObject->Fcb, IO_OP_READ, Mdl, 0, 0, &FileOffset, Cached);
}

BSTATUS IoWriteFileObject(
	PIO_STATUS_BLOCK Iosb,
	PFILE_OBJECT FileObject,
	PMDL Mdl,
	uint64_t FileOffset,
	bool Cached
)
{
	ASSERT(FileObject->Fcb);
	return IoPerformOperationFile(Iosb, FileObject->Fcb, IO_OP_WRITE, Mdl, 0, 0, &FileOffset, Cached);
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

BSTATUS OSSeekFile(HANDLE FileHandle, int64_t NewPosition, int SeekWhence)
{
	if (NewPosition == 0 && SeekWhence == IO_SEEK_CUR)
		// Nothing to do
		return STATUS_SUCCESS;
	
	if (NewPosition < 0 && SeekWhence != IO_SEEK_CUR)
		// You cannot seek to a negative offset.
		return STATUS_INVALID_PARAMETER;
	
	BSTATUS Status;
	void* FileObjectV;
	
	Status = ObReferenceObjectByHandle(FileHandle, IoFileType, &FileObjectV);
	if (FAILED(Status))
		return Status;
	
	PFILE_OBJECT FileObject = FileObjectV;
	
	uint64_t FileLength = AtLoad(FileObject->Fcb->FileLength);
	
	switch (SeekWhence)
	{
		case IO_SEEK_CUR:
		{
			// This block ensures the incrementation is atomic, and respects every limit.
			uint64_t OldOffset, NewOffset;
			do
			{
				OldOffset = NewOffset = AtLoad(FileObject->Offset);
				
				// Ensure that the addition of the NewPosition doesn't cause an underflow.
				if (NewPosition < 0 && NewOffset + NewPosition > NewOffset)
					NewOffset = 0;
				
				// Or an overflow.
				if (NewPosition > 0 && NewOffset + NewPosition < NewOffset)
					NewOffset = (uint64_t) (int64_t) -1;
			}
			while (!AtCompareExchange(&FileObject->Offset, &OldOffset, NewOffset));
			
			break;
		}
		
		case IO_SEEK_SET:
			AtStore(FileObject->Offset, NewPosition);
			break;
		
		case IO_SEEK_END:
			AtStore(FileObject->Offset, FileLength);
			break;
		
		default:
			Status = STATUS_INVALID_PARAMETER;
	}
	
	ObDereferenceObject(FileObject);
	return Status;
}
