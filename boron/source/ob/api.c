/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/api.c
	
Abstract:
	This module implements the kernel mode object manager interface in Boron.
	User mode APIs based on these are exposed in the system call layer, not
	here.
	
Author:
	iProgramInCpp - 8 December 2023
***/
#include "obp.h"

// Big mutex on the object manager.
KMUTEX ObpObjectManagerMutex;

void ObpEnterMutex()
{
	KeWaitForSingleObject(&ObpObjectManagerMutex, false, TIMEOUT_INFINITE);
}

void ObpLeaveMutex()
{
	KeReleaseMutex(&ObpObjectManagerMutex);
}

BSTATUS ObCreateObject(
	void** OutObject,
	POBJECT_DIRECTORY Parent,
	POBJECT_TYPE Type,
	const char* Name,
	int Flags,
	bool NonPaged,
	void* ParseContext,
	size_t BodySize)
{
	BSTATUS Status;
	
	ObpEnterMutex();
	
	if (ObpCheckNameInvalid(Name, 0))
	{
		ObpLeaveMutex();
		return STATUS_NAME_INVALID;
	}
	
	// Add the actual object itself:
	Status = ObiCreateObject(
		OutObject,
		Parent,
		Type,
		Name,
		Flags,
		NonPaged,
		ParseContext,
		BodySize
	);
	
	ObpLeaveMutex();
	
	return Status;
}
