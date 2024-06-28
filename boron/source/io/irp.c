/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/irp.c
	
Abstract:
	This module implements the procedures that manage IRP
	(I/O request packet) objects.
	
Author:
	iProgramInCpp - 21 February 2024
***/
#include <io.h>
#include <string.h>

// TODO: Have a pool of readily available IRP objects. This will be done later.

void IoInitializeIrp(PIRP Irp, int StackSize)
{
	memset(&Irp, 0, sizeof(IRP) + sizeof(IO_STACK_FRAME) * StackSize);
	Irp->StackSize = StackSize;
	Irp->CurrentFrameIndex = -1;
	Irp->Begun = false;
}

PIRP IoAllocateIrp(int StackSize)
{
	// TODO: Add support for pre-allocated pool of IRPs.
	
	// Maximum int8_t value is 127.
	if (StackSize > 127)
		return NULL;
	
	// For now, use pool to allocate.
	size_t Size = sizeof(IRP) + sizeof(IO_STACK_FRAME) * StackSize;
	
	PIRP Irp = MmAllocatePool(POOL_NONPAGED, Size);
	if (!Irp)
		return NULL;
	
	// Initialize the IRP.
	IoInitializeIrp(Irp, StackSize);
	
	return Irp;
}

void IoFreeIrp(PIRP Irp)
{
	if (Irp->Flags & IRP_FLAG_ZONE_ORIGINATED)
	{
		// TODO
		
		return;
	}
	
	MmFreePool(Irp);
}

// Get a pointer to the next stack location.
PIO_STACK_FRAME IoGetNextIrpStackFrame(PIRP Irp)
{
#ifdef DEBUG
	// N.B. Cast to int here to avoid uint8_t overflow
	if ((int) Irp->CurrentFrameIndex + 1 >= (int) Irp->StackSize)
	{
		KeCrash("IoGetNextIrpStackFrame: stack overflow on IRP %p with stack size %d", Irp, Irp->StackSize);
	}
#endif
	
	uint8_t Index = Irp->CurrentFrameIndex + 1;
	
	PIO_STACK_FRAME Frame = &Irp->Stack[Index];
	
	// Ensure its 'StackLocation' field matches the one 
	Frame->StackIndex = Index;
	
	return Frame;
}

PIO_STACK_FRAME IoGetIrpStackFrame(PIRP Irp, int Index)
{
#ifdef DEBUG
	if (Index >= Irp->StackSize || Index < 0)
	{
		KeCrash("IoGetIrpStackFrame: out of bounds access on IRP %p with stack size %d, tried to access frame %d",
			Irp, Irp->StackSize, Index);
	}
#endif
	
	PIO_STACK_FRAME Frame = &Irp->Stack[Index];
	
	// Ensure its 'StackLocation' field matches the one 
	Frame->StackIndex = Index;
	
	return Frame;
}

PIO_STACK_FRAME IoGetCurrentIrpStackFrame(PIRP Irp)
{
	return IoGetIrpStackFrame(Irp, Irp->CurrentFrameIndex);
}

PIRP IoGetIrpFromStackFrame(PIO_STACK_FRAME Frame)
{
	PIO_STACK_FRAME FirstFrame = &Frame[-Frame->StackIndex];
	
	return CONTAINING_RECORD(FirstFrame, IRP, Stack);
}

BSTATUS IoCallDriver(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	// TODO
	(void) DeviceObject;
	(void) Irp;
	return STATUS_UNIMPLEMENTED;
}

void IopFinishIrp(PIRP Irp, KPRIORITY Increment)
{
	// If ModeRequestor is MODE_USER, dereference the file object
	if (Irp->ModeRequestor == MODE_USER)
	{
		ASSERT(Irp->FileObject && "Must have a file object for user mode operations");
		
		ObDereferenceObject(Irp->FileObject);
	#ifdef DEBUG
		Irp->FileObject = NULL;
	#endif
	}
	
	if (Irp->CompletionEvent)
	{
		KeSetEvent(Irp->CompletionEvent, Increment);
	}
	
	if (Irp->CompletionApc)
	{
		KeInsertQueueApc(
			Irp->CompletionApc,
			Irp,
			Irp->CompletionApcContext,
			Increment
		);
	}
	
	if (Irp->Flags & IRP_FLAG_FREE_MDL)
	{
		MmUnpinPagesMdl(Irp->Mdl);
		MmFreeMdl(Irp->Mdl);
	}
	
	IoFreeIrp(Irp);
}

void IoCompleteIrpDpcLevel(PIRP Irp, BSTATUS Status, KPRIORITY Increment)
{
#ifdef DEBUG
	if (Irp->Flags & IRP_FLAG_COMPLETED)
		KeCrash("IoCompleteIrpDpcLevel: IRP was already completed");
#endif
	
	// Loop, in case we might want to complete other IRPs
	while (true)
	{
		// If no status was given, propagate one upwards from this IRP
		if (Status == STATUS_SUCCESS)
			Status = Irp->StatusBlock.Status;
		else
			// Status was given, so we will overwrite any existing status
			Irp->StatusBlock.Status = Status;
		
		// Subtract one from the associated IO count.
		//
		// The associated IO counter also includes the IRP in question.
		int IrpCount = Irp->AssociatedIrpCount;
		
		// N.B. Defer subtracting the associated IRP count for later
		
		if (IrpCount > 1)
			// There are still IRPs depending on this one, so break out
			break;
		
		bool ReturnedMoreProcessingRequired = false;
		
		// Call the current stack's completion routines
		while (Irp->CurrentFrameIndex >= 0)
		{
			PIO_STACK_FRAME Frame = &Irp->Stack[Irp->CurrentFrameIndex];
			
			ASSERT (Frame->CompletionRoutine && "Whoever reached this position should "
				"have at least set the default completion routine");
			
			BSTATUS Status = Frame->CompletionRoutine (Frame->FileControlBlock->DeviceObject, Irp, Frame->Context);
			
			if (Status == STATUS_MORE_PROCESSING_REQUIRED)
			{
				ReturnedMoreProcessingRequired = true;
				
				// NOTE: Break here as the IRP may have already been processed and freed
				
				break;
			}
			
			Irp->CurrentFrameIndex--;
		}
		
		// If the operation isn't completely done, break out and return.
		// TODO Is this it?
		if (ReturnedMoreProcessingRequired)
			return;
		
		Irp->AssociatedIrpCount--;
		
		// The IRP is completely done.
		Irp->Flags |= IRP_FLAG_COMPLETED;
		
		PIRP ParentIrp = Irp;
		
		IopFinishIrp(Irp, Increment);
		
		if (ParentIrp)
			ParentIrp = Irp;
		else
			break;
	}
}

void IoCompleteIrp(PIRP Irp, BSTATUS Status, KPRIORITY Increment)
{
	KIPL Ipl = KeRaiseIPL(IPL_DPC);
	IoCompleteIrpDpcLevel(Irp, Status, Increment);
	KeLowerIPL(Ipl);
}

// Thanks to Will (creator of MINTIA, https://github.com/xrarch/mintia) for the outline.
BSTATUS IoDispatchIrp(PIRP Irp)
{
	PKTHREAD Thread = KeGetCurrentThread();
	BSTATUS Status = STATUS_SUCCESS;
	Irp->Begun = true;
	
	bool IsMasterIrp = true;
	
	ASSERT(Irp->CurrentFrameIndex >= 0 && Irp->CurrentFrameIndex < Irp->StackSize);
	
	while (true)
	{
		PIO_STACK_FRAME Frame = IoGetCurrentIrpStackFrame(Irp);
		
		PIRP NextIrp = NULL;
		
		if (Irp->DeviceQueueEntry.Flink != Irp->DeviceQueueHead)
			NextIrp = CONTAINING_RECORD(Irp->DeviceQueueEntry.Flink, IRP, DeviceQueueEntry);
		
		ASSERT(Irp->OwnerThread == Thread);
		
		while (true)
		{
			ASSERT(Irp->CurrentFrameIndex < Irp->StackSize);
			ASSERT(Frame->FileControlBlock);
			
			PDEVICE_OBJECT DeviceObject = Frame->FileControlBlock->DeviceObject;
			PDRIVER_DISPATCH Dispatch = DeviceObject->DriverObject->DispatchTable[Frame->Function];
			
			if (!Dispatch)
			{
				IoCompleteIrp(Irp, STATUS_UNSUPPORTED_FUNCTION, 0);
				
				if (!Status)
					Status = STATUS_UNSUPPORTED_FUNCTION;
				
				break;
			}
			
			BSTATUS DispStatus = Dispatch(DeviceObject, Irp);
			
			// If the error is from an associated IRP, don't propagate an error to the caller.
			if (IsMasterIrp)
				Status = DispStatus;
			
			// If the return value was specifically next-location
			if (DispStatus == STATUS_NEXT_FRAME)
			{
				// Advance to the next frame
				Irp->CurrentFrameIndex++;
				Frame = &Frame[1];
			}
			
			// STATUS_SAME_FRAME, STATUS_NO_MORE_FRAMES or an error code may have been returned
			if (DispStatus != STATUS_SAME_FRAME)
			{
				// Don't proceed to next stack location. In fact, this IRP may have been freed.
				break;
			}
		}
		
		IsMasterIrp = false;
		
		Irp = NextIrp;
		
		if (!Irp)
		{
			// No next associated IRP, so this must have been a lone associated IRP or a master
			// IRP.  Try to process the deferred IRP list accumulated while processing the last
			// IRP.
			
			KIPL Ipl = KeRaiseIPL(IPL_APC);
			
			// Check if the list is empty.
			if (!IsListEmpty (&Thread->DeferredIrpList))
			{
				// Get the first entry.
				PLIST_ENTRY Entry = Thread->DeferredIrpList.Flink;
				Irp = CONTAINING_RECORD(Entry, IRP, DeferredIrpEntry);
				
				// On debug, ensure it's part of this thread
			#ifdef DEBUG
				ASSERT(Irp->OwnerThread == Thread);
			#endif
				
				// Re-initialize the list head.  This has the effect of clearing the list.
				// The elements of that list won't lose their links and the address of the
				// current thread's deferred IRP list sentinel can still be used as an end
				// marker.
				InitializeListHead(&Thread->DeferredIrpList);
			}
			
			KeLowerIPL(Ipl);
			
			if (!Irp)
				break;
		}
	}
	
	return Status;
}

BSTATUS IoSendSynchronousIrp(PIRP Irp)
{
	ASSERT(Irp->CompletionEvent && "TODO: Is CompletionEvent supposed to be set with this on?");
	
	// Initialize an event so that we can wait for the IRP to complete.
	KEVENT Event;
	KeInitializeEvent(&Event, EVENT_SYNCHRONIZATION, false);
	Irp->CompletionEvent = &Event;
	
	// Dispatch the IRP.
	BSTATUS Status = IoDispatchIrp(Irp);
	
	switch (Status)
	{
		case STATUS_SUCCESS:
			return Status;
		
		case STATUS_PENDING:
		{
			// Wait for the event to complete.
			Status = KeWaitForSingleObject(&Event, false, Irp->Timeout);
			
			if (Status != STATUS_SUCCESS)
			{
				// TODO: Cancel IRP
				DbgPrint("TODO: Timed out IRP %p that returned STATUS_PENDING", Irp);
				
				// TODO: Free IRP
				return Status;
			}
			
			// TODO: Free IRP
			return Irp->StatusBlock.Status;
		}
		
		default:
		{
			KeCrash("TODO: IoSendSynchronousIrp: Add handler for status code %d", Status);
			return STATUS_UNIMPLEMENTED;
		}
	}
}
