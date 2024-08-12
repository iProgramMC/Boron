/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ex/wait.c
	
Abstract:
	This module implements the Wait Objects system calls.
	
Author:
	iProgramInCpp - 11 August 2024
***/
#include "exp.h"

// Ensure the Ex definitions match the Ke definitions.
static_assert((int)WAIT_ALL_OBJECTS == (int) WAIT_TYPE_ALL);
static_assert((int)WAIT_ANY_OBJECT  == (int) WAIT_TYPE_ANY);
static_assert(WAIT_TIMEOUT_INFINITE == TIMEOUT_INFINITE);

static bool ExpIsDispatchObject(void* Object)
{
	POBJECT_TYPE Type = ObGetObjectType(Object);

	return Type == ExMutexObjectType     ||
	       Type == ExEventObjectType     ||
	       Type == ExSemaphoreObjectType ||
	       Type == ExTimerObjectType     ||
	       Type == ExThreadObjectType    ||
	       Type == ExProcessObjectType;
}

BSTATUS OSWaitForSingleObject(
	HANDLE Handle,
	bool Alertable,
	int TimeoutMS
)
{
	BSTATUS Status = STATUS_SUCCESS;
	void* Object = NULL;
	
	Status = ObReferenceObjectByHandle(Handle, NULL, &Object);
	if (FAILED(Status))
		return Status;
	
	if (!ExpIsDispatchObject(Object))
	{
		Status = STATUS_INVALID_PARAMETER;
		goto End;
	}
	
	Status = KeWaitForSingleObject(
		Object,
		Alertable,
		TimeoutMS
	);
	
End:
	ObDereferenceObject(Object);
	return Status;
}

BSTATUS OSWaitForMultipleObjects(
	int ObjectCount,
	PHANDLE ObjectsArray,
	int WaitType,
	bool Alertable,
	int TimeoutMS
)
{
	BSTATUS Status = STATUS_SUCCESS;
	PKWAIT_BLOCK WaitBlocks = NULL;
	void** Objects = NULL;
	int ReferencedObjectCount = 0;
	
	// Check the validity of parameters immediately.
	if (ObjectCount > MAXIMUM_WAIT_BLOCKS ||
		ObjectCount < 0 ||
		!ObjectsArray ||
		(WaitType != WAIT_ALL_OBJECTS && WaitType != WAIT_ANY_OBJECT))
		return STATUS_INVALID_PARAMETER;
	
	Objects = MmAllocatePool(POOL_NONPAGED, ObjectCount * sizeof (void*));
	if (!Objects)
	{
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto End;
	}
	
	if (ObjectCount > THREAD_WAIT_BLOCKS)
	{
		// Also try allocating the wait blocks.
		WaitBlocks = MmAllocatePool(POOL_NONPAGED, ObjectCount * sizeof(KWAIT_BLOCK));
		if (!WaitBlocks)
		{
			Status = STATUS_INSUFFICIENT_MEMORY;
			goto End;
		}
	}
	
	// Start referencing the handles.
	for (int i = 0; i < ObjectCount; i++)
	{
		HANDLE Handle;
		Status = MmSafeCopy(&Handle, ObjectsArray + i, sizeof(HANDLE), KeGetPreviousMode(), false);
		if (FAILED(Status))
			goto End;
		
		Status = ObReferenceObjectByHandle(Handle, NULL, &Objects[i]);
		if (FAILED(Status))
			goto End;
		
		// Check the objects' validity.
		if (!ExpIsDispatchObject(Objects[i]))
		{
			Status = STATUS_INVALID_PARAMETER;
			goto End;
		}
		
		ReferencedObjectCount = i;
	}
	
	// Ensure that the objects aren't specified more than once.
	for (int i = 0; i < ObjectCount; i++)
	{
		for (int j = i + 1; j < ObjectCount; j++)
		{
			if (Objects[i] == Objects[j])
			{
				Status = STATUS_INVALID_PARAMETER;
				goto End;
			}
		}
	}
	
	// OK! Everything was referenced correctly, let's do the wait now.
	Status = KeWaitForMultipleObjects(
		ObjectCount,
		Objects,
		WaitType,
		Alertable,
		TimeoutMS,
		WaitBlocks
	);
	
End:
	ASSERT(Objects || !ReferencedObjectCount);
	
	for (int i = 0; i < ReferencedObjectCount; i++)
		ObDereferenceObject(Objects[i]);
	
	if (WaitBlocks)
		MmFreePool(WaitBlocks);
	
	if (Objects)
		MmFreePool(Objects);
	
	return Status;
}
