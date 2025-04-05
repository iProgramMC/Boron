/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ex/object.h
	
Abstract:
	This header defines prototypes for executive dispatch
	object manipulation functions.
	
Author:
	iProgramInCpp - 9 August 2024
***/
#pragma once

#include <ob.h>

#ifdef KERNEL

// Does the same operation as ObReferenceObjectByHandle, except translates
// special handle values to references to the actual object.
// (e.g. CURRENT_PROCESS_HANDLE is translated to the current process)
BSTATUS ExReferenceObjectByHandle(HANDLE Handle, POBJECT_TYPE ExpectedType, void** OutObject);

typedef BSTATUS(*EX_OBJECT_CREATE_METHOD)(void* Object, void* Context);

// Create an object based on a user system service call.  This handles safety in all aspects, including
// copying the object attributes to kernel pool, dereferencing the root directory, copying the handle into
// the OutHandle pointer, etc.
BSTATUS ExCreateObjectUserCall(
	PHANDLE OutHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	POBJECT_TYPE ObjectType,
	size_t ObjectBodySize,
	EX_OBJECT_CREATE_METHOD CreateMethod,
	void* CreateContext
);

// Opens an object from a user system service call.  This ensures that code is not duplicated.
BSTATUS ExOpenObjectUserCall(
	PHANDLE OutHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	POBJECT_TYPE ObjectType
);

#endif

#define EX_DISPATCH_BOOST ((KPRIORITY) 1)

enum
{
	WAIT_ALL_OBJECTS,
	WAIT_ANY_OBJECT,
	__WAIT_TYPE_COUNT,
};

#define WAIT_TIMEOUT_INFINITE (0x7FFFFFFF)

BSTATUS OSClose(HANDLE Handle);

BSTATUS OSCreateMutex(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes);

BSTATUS OSOpenMutex(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes);

BSTATUS OSReleaseMutex(HANDLE MutexHandle);

BSTATUS OSQueryMutex(HANDLE MutexHandle, int* MutexState);

BSTATUS OSCreateEvent(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes, int EventType, bool State);

BSTATUS OSOpenEvent(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes);

BSTATUS OSSetEvent(HANDLE EventHandle);

BSTATUS OSResetEvent(HANDLE EventHandle);

BSTATUS OSPulseEvent(HANDLE EventHandle);

BSTATUS OSQueryEvent(HANDLE EventHandle, int* EventState);

BSTATUS OSWaitForMultipleObjects(
	int ObjectCount,
	PHANDLE ObjectsArray,
	int WaitType,
	bool Alertable,
	int TimeoutMS
);

BSTATUS OSWaitForSingleObject(
	HANDLE Handle,
	bool Alertable,
	int TimeoutMS
);