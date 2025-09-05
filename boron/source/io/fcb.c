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

PFCB IoAllocateFcb(PIO_DISPATCH_TABLE Dispatch, size_t ExtensionSize, bool NonPaged)
{
	// Allocate the actual FCB object.
	PFCB Fcb = MmAllocatePool (NonPaged ? POOL_NONPAGED : POOL_PAGED, sizeof(FCB) + ExtensionSize);
	
	if (!Fcb)
		return NULL;
	
	Fcb->DispatchTable = Dispatch;
	Fcb->ExtensionSize = ExtensionSize;
	
	MmInitializeCcb(&Fcb->PageCache);
	ExInitializeRwLock(&Fcb->RwLock);
	KeInitializeMutex(&Fcb->ViewCacheMutex, 0);
	
	memset(Fcb->Extension, 0, Fcb->ExtensionSize);
	return Fcb;
}

void IoFreeFcb(PFCB Fcb)
{
	ExDeinitializeRwLock(&Fcb->RwLock);
	MmTearDownCcb(&Fcb->PageCache);
	MmFreePool(Fcb);
}

void IoReferenceFcb(PFCB Fcb)
{
	IO_REFERENCE_METHOD RefMethod = Fcb->DispatchTable->Dereference;
	
	if (RefMethod)
		RefMethod(Fcb);
}

void IoDereferenceFcb(PFCB Fcb)
{
	IO_DEREFERENCE_METHOD DerefMethod = Fcb->DispatchTable->Dereference;
	
	if (DerefMethod)
		DerefMethod(Fcb);
}
