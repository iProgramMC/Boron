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
	memset(&Irp, 0, sizeof(IRP) + sizeof(IO_STACK_LOCATION) * StackSize);
	Irp->StackSize = StackSize;
	Irp->CurrentLocation = 0;
}

PIRP IoAllocateIrp(int StackSize)
{
	// For now, use pool to allocate.
	size_t Size = sizeof(IRP) + sizeof(IO_STACK_LOCATION) * StackSize;
	
	PIRP Irp = MmAllocatePool(POOL_NONPAGED, Size);
	if (!Irp)
		return NULL;
	
	// Initialize the IRP.
	IoInitializeIrp(Irp, StackSize);
	
	return Irp;
}
