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

// 
BSTATUS ObpAllocateObject(
	POBJECT_TYPE Type,
	const char* Name,
	size_t BodySize,
	bool NonPaged,
	void* ParseContext,
	int Flags,
	POBJECT_HEADER* OutObjectHeader
);

// Initialization steps
bool ObpInitializeBasicMutexes();
bool ObpInitializeBasicTypes();

// Mutexes
void ObpEnterObjectTypeMutex();
void ObpLeaveObjectTypeMutex();
void ObpEnterRootDirectoryMutex();
void ObpLeaveRootDirectoryMutex();



