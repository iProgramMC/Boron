/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/obp.h
	
Abstract:
	This header file defines private object manager data,
	and includes some header files that are usually included.
	
Author:
	iProgramInCpp - 7 December 2023
***/
#pragma once

#include <ob.h>
#include <string.h>

#define OB_MUTEX_LEVEL_HANDLE_TABLE (1)
#define OB_MUTEX_LEVEL_DIRECTORY    (2)
#define OB_MUTEX_LEVEL_OBJECT_TYPES (3)

#define OB_MAX_LOOP (16) // Completely arbitrary

extern KMUTEX ObpRootDirectoryMutex;

// Private functions
BSTATUS ObpAllocateObject(
	POBJECT_TYPE Type,
	const char* Name,
	size_t BodySize,
	void* ParseContext,
	int Flags,
	POBJECT_HEADER* OutObjectHeader
);

BSTATUS ObpNormalizeParentDirectoryAndName(
	POBJECT_DIRECTORY* ParentDirectory,
	const char** Name
);

// Gets a pointer to the type of this object.
static inline ALWAYS_INLINE
POBJECT_TYPE ObpGetObjectType(void* Object)
{
	return OBJECT_GET_HEADER(Object)->NonPagedObjectHeader->ObjectType;
}

// Performs a path lookup, returns a referenced object if found.
BSTATUS ObpLookUpObjectPath(
	void* InitialParseObject,
	const char* ObjectName,
	POBJECT_TYPE ExpectedType,
	int LoopCount,
	int OpenFlags,
	void** FoundObject
);

// Initialization steps
bool ObpInitializeBasicMutexes();
bool ObpInitializeBasicTypes();
bool ObpInitializeRootDirectory();
bool ObpInitializeReaperThread();

// Mutexes
void ObpEnterObjectTypeMutex();
void ObpLeaveObjectTypeMutex();
void ObpEnterRootDirectoryMutex();
void ObpLeaveRootDirectoryMutex();



