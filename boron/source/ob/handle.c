/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ob/handle.c
	
Abstract:
	This module implements functions to manipulate the object
	handle table for the object manager.
	
Author:
	iProgramInCpp - 11 June 2024
***/
#include "obp.h"
#include <ex.h>

BSTATUS ObReferenceObjectByHandle(HANDLE Handle, void** OutObject)
{
	BSTATUS Status;
	PEPROCESS Process = PsGetCurrentProcess();
	
	Status = ExAcquireSharedRwLock(&Process->AddressLock, false, false, false);
	if (FAILED(Status))
		return Status;
	
	// Look for the handle in the table.
	void* Object = NULL;
	Status = ExGetPointerFromHandle(Process->HandleTable, Handle, &Object);
	if (FAILED(Status))
		return Status;
	
	// If it returned STATUS_SUCCESS, it should have returned a valid object.
	ASSERT(Object);
	
	// Reference the object.
	ObReferenceByPointerObject(Object);
	
	*OutObject = Object;
	
	return STATUS_SUCCESS;
}
