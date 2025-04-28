/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ex/mutex.c
	
Abstract:
	This module implements the Mutex executive dispatch object.
	
Author:
	iProgramInCpp - 9 August 2024
***/
#include "exp.h"

POBJECT_TYPE ExMutexObjectType;

bool ExpCreateMutexType()
{
	OBJECT_TYPE_INFO ObjectInfo;
	memset (&ObjectInfo, 0, sizeof ObjectInfo);
	
	ObjectInfo.NonPagedPool = true;
	ObjectInfo.MaintainHandleCount = true;
	
	BSTATUS Status = ObCreateObjectType(
		"Mutex",
		&ObjectInfo,
		&ExMutexObjectType
	);
	
	if (FAILED(Status))
	{
		DbgPrint("Could not create Mutex type: %d", Status);
		return false;
	}
	
	return true;
}

BSTATUS OSReleaseMutex(HANDLE MutexHandle)
{
	BSTATUS Status;
	void* MutexV;
	
	Status = ObReferenceObjectByHandle(MutexHandle, ExMutexObjectType, &MutexV);
	
	if (SUCCEEDED(Status))
	{
		KeReleaseMutex((PKMUTEX) MutexV);
		ObDereferenceObject(MutexV);
	}
	
	return Status;
}

BSTATUS OSQueryMutex(HANDLE MutexHandle, int* MutexState)
{
	if (!MutexState)
		return STATUS_INVALID_PARAMETER;
	
	BSTATUS Status;
	void* MutexV;
	
	Status = MmProbeAddress(MutexState, sizeof(int), true, KeGetPreviousMode());
	if (FAILED(Status))
		return Status;
	
	Status = ObReferenceObjectByHandle(MutexHandle, ExMutexObjectType, &MutexV);
	
	if (SUCCEEDED(Status))
	{
		int State = KeReadStateMutex((PKMUTEX) MutexV);
		Status = MmSafeCopy(MutexState, &State, sizeof(int), KeGetPreviousMode(), true);
		ObDereferenceObject(MutexV);
	}
	
	return Status;
}

static BSTATUS ExpInitializeMutexObject(void* MutexV, UNUSED void* Context)
{
	KeInitializeMutex((PKMUTEX) MutexV, EX_MUTEX_LEVEL);
	return STATUS_SUCCESS;
}

BSTATUS OSCreateMutex(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes)
{
	return ExCreateObjectUserCall(OutHandle, ObjectAttributes, ExMutexObjectType, sizeof(KMUTEX), ExpInitializeMutexObject, NULL, POOL_NONPAGED, false);
}

BSTATUS OSOpenMutex(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes)
{
	return ExOpenObjectUserCall(OutHandle, ObjectAttributes, ExMutexObjectType);
}
