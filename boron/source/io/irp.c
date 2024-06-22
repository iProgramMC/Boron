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
	Irp->CurrentFrameIndex = 0;
}

PIRP IoAllocateIrp(int StackSize)
{
	// TODO: Add support for pre-allocated pool of IRPs.
	
	// Maximum uint8_t value is 255.
	if (StackSize > 255)
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

BSTATUS IoDispatchIrp(PIRP Irp)
{
	Irp->Flags |= IRP_FLAG_PENDING;
	
	// The logic would go as follows:
	// Call the current frame in the stack.
	// If it issued an IoCallDriver, execution should continue downwards.
	// Otherwise, it should jump back up.
	
	return STATUS_UNIMPLEMENTED;
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
