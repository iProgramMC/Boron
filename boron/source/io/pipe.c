/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	io/pipe.c
	
Abstract:
	This implements the I/O pipe object.  The pipe object
	is implemented using a circular ring buffer of variable
	size.
	
Author:
	iProgramInCpp - 1 September 2025
***/
#include "iop.h"

// Priority increment given to writer and reader threads when an event happens
#define PIPE_INCREMENT (3)

// A pipe is an FCB whose extension is this structure.
typedef struct _PIPE
{
	KMUTEX Mutex;
	KEVENT QueueEmptyEvent;
	KEVENT QueueFullEvent;
	size_t Head;
	size_t Tail;
	size_t BufferSize;
	bool Terminated;
	char Buffer[];
}
PIPE, *PPIPE;

void IopInitializePipe(PPIPE Pipe, size_t BufferSize)
{
	Pipe->BufferSize = BufferSize;
	Pipe->Head = 0;
	Pipe->Tail = 0;
	Pipe->Terminated = false;
	KeInitializeMutex(&Pipe->Mutex, 0);
	KeInitializeEvent(&Pipe->QueueEmptyEvent, EVENT_NOTIFICATION, false);
	KeInitializeEvent(&Pipe->QueueFullEvent,  EVENT_NOTIFICATION, false);
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
	
	// Then, check if there is any data.
	if (Pipe->Terminated)
	{
		Iosb->Status = STATUS_END_OF_FILE;
		goto Finish;
	}
	
	size_t ByteCount = MdlBuffer->ByteCount;
	for (size_t i = 0; i < ByteCount; )
	{
		Iosb->BytesRead = i;
		
		if (Pipe->Terminated)
		{
			// Well, simply return that we read this many bytes, and that's it.
			return STATUS_SUCCESS;
		}
		
		if (Pipe->Head == Pipe->Tail)
		{
			// No data, need to wait for more.
			
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
Finish:
	Iosb->Status = Status;
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
	
	// Then, check if there is any data.
	if (Pipe->Terminated)
	{
		Iosb->Status = STATUS_END_OF_FILE;
		goto Finish;
	}
	
	size_t ByteCount = MdlBuffer->ByteCount;
	for (size_t i = 0; i < ByteCount; )
	{
		Iosb->BytesWritten = i;
		
		if (Pipe->Terminated)
		{
			// Well, simply return that we read this many bytes, and that's it.
			return STATUS_SUCCESS;
		}
		
		size_t FreeSpace;
		if (Pipe->Head >= Pipe->Tail)
			FreeSpace = Pipe->BufferSize - 1 - (Pipe->Head - Pipe->Tail);
		else
			FreeSpace = Pipe->Tail - Pipe->Head - 1;
		
		if (FreeSpace == 0)
		{
			// No free space, so wait.
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
Finish:
	Iosb->Status = Status;
	return Status;
}

IO_DISPATCH_TABLE IopPipeDispatchTable = {
	.Flags = 0,
	//.Dereference = IopDereferencePipe,
	.Read = IopReadPipe,
	.Write = IopWritePipe,
};
