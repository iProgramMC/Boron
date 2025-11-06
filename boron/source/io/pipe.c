/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	io/pipe.c
	
Abstract:
	This implements the I/O pipe object.  The pipe object
	is implemented using a circular ring buffer of variable
	size.
	
	This implementation supports anonymous pipes and named
	pipes.  Named pipes are implemented using a special object
	type called NamedPipe which parses into a file object when
	looked up.
	
Author:
	iProgramInCpp - 1 September 2025
***/
#include "iop.h"
#include <ex/internal.h>

// Priority increment given to writer and reader threads when an event happens
#define PIPE_INCREMENT (3)

// Default pipe size.
#define DEFAULT_PIPE_SIZE (1024)

// Maximum pipe size.
#define MAX_PIPE_SIZE (32768)

// A pipe is an FCB whose extension is this structure.
typedef struct _PIPE
{
	size_t ReferenceCount;
	KMUTEX Mutex;
	KEVENT QueueEmptyEvent;
	KEVENT QueueFullEvent;
	size_t Head;
	size_t Tail;
	size_t BufferSize;
	char Buffer[];
}
PIPE, *PPIPE;

void IopInitializePipe(PPIPE Pipe, size_t BufferSize)
{
	Pipe->BufferSize = BufferSize;
	Pipe->Head = 0;
	Pipe->Tail = 0;
	Pipe->ReferenceCount = 1;
	KeInitializeMutex(&Pipe->Mutex, 0);
	KeInitializeEvent(&Pipe->QueueEmptyEvent, EVENT_NOTIFICATION, false);
	KeInitializeEvent(&Pipe->QueueFullEvent,  EVENT_NOTIFICATION, false);
	
	DbgPrint("IopInitializePipe: %p created with RC 1", CONTAINING_RECORD(Pipe, FCB, Extension));
}

// Reads data from a pipe.
BSTATUS IopReadPipe(PIO_STATUS_BLOCK Iosb, PFCB Fcb, UNUSED uint64_t Offset, PMDL MdlBuffer, UNUSED uint32_t Flags)
{
	// The FCB does not need to be locked.  We have our own mutex.
	IoUnlockFcb(Fcb);
	
	BSTATUS Status = STATUS_SUCCESS;
	ASSERT(MdlBuffer->Flags & MDL_FLAG_WRITE);
	PPIPE Pipe = (PPIPE) Fcb->Extension;
	
	Iosb->BytesRead = 0;
	Iosb->Status = Status;
	
	// Lock the pipe for the first time.
	Status = KeWaitForSingleObject(&Pipe->Mutex, true, TIMEOUT_INFINITE, KeGetPreviousMode());
	if (FAILED(Status))
		goto Finish;
	
	size_t ByteCount = MdlBuffer->ByteCount;
	for (size_t i = 0; i < ByteCount; )
	{
		Iosb->BytesRead = i;
		
		if (Pipe->Head == Pipe->Tail)
		{
			// If the pipe has only one owner, return prematurely, otherwise a deadlock will ensue.
			// NOTE: This will NOT work if file objects aren't duplicated!
			if (Pipe->ReferenceCount == 1)
			{
				DbgPrint("PIPE: Declaring end of file because there is only one owner and there is no data left!");
				Status = STATUS_END_OF_FILE;
				goto FinishRelease;
			}
			
			// No data. Check if we need to wait for more.
			// Either:
			// - the full IO_RW_NONBLOCK is specified, at which point the operation will abort
			// - IO_RW_NONBLOCK_UNLESS_EMPTY is specified.  in this case, wait only if the queue
			//   was empty when we entered (so we didn't read any bytes)
			if ((Flags & IO_RW_NONBLOCK) || ((Flags & IO_RW_NONBLOCK_UNLESS_EMPTY) && i != 0))
			{
				Iosb->Status = STATUS_BLOCKING_OPERATION;
				goto FinishRelease;
			}
			
			// NOTE: The following block is performed atomically, that is, there is no point in time
			// where the mutex is unlocked but the wait isn't being performed, between these two lines.
			KeReleaseMutexWait(&Pipe->Mutex);
			Status = KeWaitForSingleObject(&Pipe->QueueEmptyEvent, true, TIMEOUT_INFINITE, KeGetPreviousMode());
			
			if (FAILED(Status))
				// Wait failed, likely because of an interruption.
				goto Finish;
			
			Status = KeWaitForSingleObject(&Pipe->Mutex, true, TIMEOUT_INFINITE, KeGetPreviousMode());
			if (FAILED(Status))
				goto Finish;
			
			continue;
		}
		
		// There is data... but how much?
		size_t AvailableData;
		if (Pipe->Head >= Pipe->Tail)
		{
			AvailableData = Pipe->Head - Pipe->Tail;
		}
		else
		{
			AvailableData = Pipe->Head + Pipe->BufferSize - Pipe->Tail;
		}
		
		size_t ReadSize = ByteCount - i;
		if (ReadSize > AvailableData)
		{
			// We can't consume all data in one go.
			// Read what we can, then re-enter into the loop.
			ReadSize = AvailableData;
		}
		
		size_t TailAfterRead = Pipe->Tail + ReadSize;
		if (TailAfterRead >= Pipe->BufferSize)
			TailAfterRead %= Pipe->BufferSize;
		
		if (Pipe->Tail <= TailAfterRead)
		{
			// The tail can catch up to the head without overflowing.
			MmCopyIntoMdl(MdlBuffer, i, Pipe->Buffer + Pipe->Tail, ReadSize);
		}
		else
		{
			// The data is split into two - tail->buffersize and 0->head
			size_t TailSize = Pipe->BufferSize - Pipe->Tail;
			ASSERT(TailSize < ReadSize);
			ASSERT(TailSize < AvailableData);
			size_t HeadSize = ReadSize - TailSize;
			
			MmCopyIntoMdl(MdlBuffer, i, Pipe->Buffer + Pipe->Tail, TailSize);
			MmCopyIntoMdl(MdlBuffer, i + TailSize, Pipe->Buffer, HeadSize);
		}
		
		Pipe->Tail = TailAfterRead;
		
		i += ReadSize;
		
		// Since some data was consumed, signal writers to write
		KePulseEvent(&Pipe->QueueFullEvent, PIPE_INCREMENT);
	}
	
	Status = STATUS_SUCCESS;
	
FinishRelease:
	KeReleaseMutex(&Pipe->Mutex);
	
Finish:
	Iosb->Status = Status;
	
	// Relock the rwlock because the caller expects us to do so.
	IoLockFcbShared(Fcb);
	return Status;
}

// Writes data to a pipe.
BSTATUS IopWritePipe(PIO_STATUS_BLOCK Iosb, PFCB Fcb, UNUSED uint64_t Offset, PMDL MdlBuffer, UNUSED uint32_t Flags)
{
	// The FCB does not need to be locked.  We have our own mutex.
	IoUnlockFcb(Fcb);
	
	BSTATUS Status = STATUS_SUCCESS;
	ASSERT(~MdlBuffer->Flags & MDL_FLAG_WRITE);
	PPIPE Pipe = (PPIPE) Fcb->Extension;
	
	Iosb->BytesWritten = 0;
	Iosb->Status = Status;
	
	// Lock the pipe for the first time.
	Status = KeWaitForSingleObject(&Pipe->Mutex, true, TIMEOUT_INFINITE, KeGetPreviousMode());
	if (FAILED(Status))
		goto Finish;
	
	size_t ByteCount = MdlBuffer->ByteCount;
	for (size_t i = 0; i < ByteCount; )
	{
		Iosb->BytesWritten = i;
		
		size_t FreeSpace;
		if (Pipe->Head >= Pipe->Tail)
			FreeSpace = Pipe->BufferSize - 1 - (Pipe->Head - Pipe->Tail);
		else
			FreeSpace = Pipe->Tail - Pipe->Head - 1;
		
		if (FreeSpace == 0)
		{
			// No free space, so wait.
			if (Pipe->ReferenceCount == 1)
			{
				DbgPrint("PIPE: Declaring end of file because there is only one owner and the pipe is full!");
				Status = STATUS_END_OF_FILE;
				goto FinishRelease;
			}
			
			// Full of data, need to wait to send more.
			if (Flags & IO_RW_NONBLOCK)
			{
				Status = STATUS_BLOCKING_OPERATION;
				goto FinishRelease;
			}
			
			KeReleaseMutexWait(&Pipe->Mutex);
			
			Status = KeWaitForSingleObject(&Pipe->QueueFullEvent, true, TIMEOUT_INFINITE, KeGetPreviousMode());
			if (FAILED(Status))
				goto Finish;
			
			Status = KeWaitForSingleObject(&Pipe->Mutex, true, TIMEOUT_INFINITE, KeGetPreviousMode());
			if (FAILED(Status))
				goto Finish;
			
			continue;
		}
		
		// There is free space.
		size_t WriteSize = ByteCount - i;
		if (WriteSize > FreeSpace)
		{
			// We can't write all the data in one go.
			// Write what we can, then re-enter the loop.
			WriteSize = FreeSpace;
		}
		
		size_t HeadAfterWrite = Pipe->Head + WriteSize;
		if (HeadAfterWrite >= Pipe->BufferSize)
			HeadAfterWrite %= Pipe->BufferSize;
		
		if (Pipe->Head <= HeadAfterWrite)
		{
			// The head can catch up to the tail without overflowing.
			MmCopyFromMdl(MdlBuffer, i, Pipe->Buffer + Pipe->Head, WriteSize);
		}
		else
		{
			// The data is split into two - head->buffersize and 0->tail
			size_t HeadSize = Pipe->BufferSize - Pipe->Head;
			ASSERT(HeadSize < WriteSize);
			ASSERT(HeadSize < FreeSpace);
			size_t TailSize = WriteSize - HeadSize;
			
			MmCopyFromMdl(MdlBuffer, i, Pipe->Buffer + Pipe->Head, HeadSize);
			MmCopyFromMdl(MdlBuffer, i + HeadSize, Pipe->Buffer, TailSize);
		}
		
		Pipe->Head = HeadAfterWrite;
		ASSERT(Pipe->Tail != Pipe->Head);
		
		i += WriteSize;
		
		// Since some data was written, signal readers to read
		KePulseEvent(&Pipe->QueueEmptyEvent, PIPE_INCREMENT);
	}
	
	Status = STATUS_SUCCESS;
	
FinishRelease:
	KeReleaseMutex(&Pipe->Mutex);
	
Finish:
	Iosb->Status = Status;
	
	// Relock the rwlock because the caller expects us to do so.
	IoLockFcbShared(Fcb);
	return Status;
}

void IopReferencePipe(PFCB Fcb)
{
	PPIPE Pipe = (PPIPE) Fcb->Extension;
	
	DbgPrint("IopReferencePipe: %p (RA: %p, %p) %zu", Fcb, __builtin_return_address(1), __builtin_return_address(2), Pipe->ReferenceCount + 1);
	AtAddFetch(Pipe->ReferenceCount, 1);
}

void IopDereferencePipe(PFCB Fcb)
{
	PPIPE Pipe = (PPIPE) Fcb->Extension;
	DbgPrint("IopDereferencePipe: %p (RA: %p, %p) %zu", Fcb, __builtin_return_address(1), __builtin_return_address(2), Pipe->ReferenceCount - 1);
	
	if (AtAddFetch(Pipe->ReferenceCount, -1) == 0)
	{
		DbgPrint("IopDereferencePipe: Freeing pipe FCB");
		IoFreeFcb(Fcb);
	}
}

IO_DISPATCH_TABLE IopPipeDispatchTable =
{
	.Flags = 0,
	.Reference = IopReferencePipe,
	.Dereference = IopDereferencePipe,
	.Read = IopReadPipe,
	.Write = IopWritePipe,
};

BSTATUS IoCreatePipeObject(PFILE_OBJECT* OutFileObject, PFCB* OutFcb, POBJECT_ATTRIBUTES ObjectAttributes, size_t BufferSize)
{
	if (ObjectAttributes)
	{
		// Named pipes are currently not supported.  TODO
		return STATUS_UNIMPLEMENTED;
	}
	
	PFCB Fcb = IoAllocateFcb(&IopPipeDispatchTable, sizeof(PIPE) + BufferSize, false);
	if (!Fcb)
		return STATUS_INSUFFICIENT_MEMORY;
	
	// Initialize the pipe with the specified buffer size, create its corresponding
	// file object, and then insert it into the handle table.
	IopInitializePipe((PPIPE) Fcb->Extension, BufferSize);
	
	BSTATUS Status = IoCreateFileObject(Fcb, OutFileObject, 0, 0);
	if (FAILED(Status))
	{
		IoFreeFcb(Fcb);
		return Status;
	}
	
	*OutFcb = Fcb;
	return Status;
}

BSTATUS IoCreatePipe(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes, size_t BufferSize)
{
	PFCB Fcb = NULL;
	PFILE_OBJECT FileObject = NULL;
	BSTATUS Status = IoCreatePipeObject(&FileObject, &Fcb, ObjectAttributes, BufferSize);
	
	Status = ObInsertObject(FileObject, OutHandle, 0);
	if (FAILED(Status))
	{
		ObDereferenceObject(FileObject);
		IoFreeFcb(Fcb);
		return Status;
	}
	
	IopDereferencePipe(Fcb);
	ObDereferenceObject(FileObject);
	return Status;
}

// See ExCreateObjectUserCall for an in-depth explanation on how this is structured.
BSTATUS OSCreatePipe(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes, size_t BufferSize)
{
	BSTATUS Status = STATUS_SUCCESS;
	bool CopyAttrs = false;
	OBJECT_ATTRIBUTES AttributesCopy;
	
	if (BufferSize == 0)
		BufferSize = DEFAULT_PIPE_SIZE;
	
	if (BufferSize < 64 || BufferSize > MAX_PIPE_SIZE)
		return STATUS_INVALID_PARAMETER;
	
	// If needed, copy the attributes on the stack so that they're safe to use
	// by IoCreatePipe.  This is because IoCreatePipe is kernel-mode only, so
	// faults on user mode are unacceptable.
	if (ObjectAttributes)
	{
		if (KeGetPreviousMode() == MODE_KERNEL)
		{
			AttributesCopy = *ObjectAttributes;
		}
		else
		{
			CopyAttrs = true;
			Status = ExCopySafeObjectAttributes(&AttributesCopy, ObjectAttributes);
			if (FAILED(Status))
				return Status;
		}
	}
	
	HANDLE Handle;
	Status = IoCreatePipe(&Handle, ObjectAttributes ? &AttributesCopy : NULL, BufferSize);
	if (SUCCEEDED(Status))
	{
		Status = MmSafeCopy(OutHandle, &Handle, sizeof(HANDLE), KeGetPreviousMode(), true);
		
		if (FAILED(Status))
			OSClose(Handle);
	}
	
	if (CopyAttrs)
		ExDisposeCopiedObjectAttributes(&AttributesCopy);
	
	return Status;
}
