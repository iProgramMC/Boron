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

// Private functions
BSTATUS ObpAllocateObject(
	POBJECT_TYPE Type,
	const char* Name,
	size_t BodySize,
	bool NonPaged,
	void* ParseContext,
	int Flags,
	POBJECT_HEADER* OutObjectHeader
);

BSTATUS ObpAddObjectToDirectory(
	POBJECT_DIRECTORY Directory,
	void* Object
);

// Adds 1 to the internal reference count of the object.
void ObpReferenceObject(void* Object);

// Takes a reference away from the internal ref count of the object.
// If the reference count hits zero, the object will be deleted.
void ObpDereferenceObject(void* Object);

// Takes a reference away from the internal ref count of the object.
// The object won't get deleted if the reference count hits zero.
void ObpDereferenceObjectNoDelete(void* Object);

// Initialization steps
bool ObpInitializeBasicMutexes();
bool ObpInitializeBasicTypes();
bool ObpInitializeRootDirectory();

// Mutexes
void ObpEnterObjectTypeMutex();
void ObpLeaveObjectTypeMutex();
void ObpEnterRootDirectoryMutex();
void ObpLeaveRootDirectoryMutex();



