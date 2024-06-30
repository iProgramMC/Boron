/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/fcb.c
	
Abstract:
	This module implements the I/O manager specific part of the FCB structure.
	
Author:
	iProgramInCpp - 29 June 2024
***/
#include "iop.h"

PFCB IoAllocateFcb(PDEVICE_OBJECT DeviceObject, size_t ExtensionSize, bool NonPaged)
{
	// Allocate the actual FCB object.
	PFCB Fcb = MmAllocatePool (NonPaged ? POOL_NONPAGED : POOL_PAGED, sizeof(FCB) + ExtensionSize);
	
	if (!Fcb)
		return NULL;
	
	Fcb->DeviceObject = DeviceObject;
	Fcb->DispatchTable = DeviceObject->DispatchTable;
	Fcb->ExtensionSize = ExtensionSize;
	
	ExInitializeRwLock (&Fcb->RwLock);
	
	return Fcb;
}

void IoFreeFcb(PFCB Fcb)
{
	ExDeinitializeRwLock(&Fcb->RwLock);
	
	MmFreePool(Fcb);
}
