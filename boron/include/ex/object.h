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

typedef struct
{
	HANDLE RootDirectory;
	const char* ObjectName;
	size_t ObjectNameLength;
}
OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

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

BSTATUS OSOpenMutex(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes, int OpenFlags);

BSTATUS OSReleaseMutex(HANDLE MutexHandle);

BSTATUS OSQueryMutex(HANDLE MutexHandle, int* MutexState);

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