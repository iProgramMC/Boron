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

// Default object types provided by the system.
extern POBJECT_TYPE ObpObjectTypeType;
extern POBJECT_TYPE ObpDirectoryType;
extern POBJECT_TYPE ObpSymbolicLinkType;

extern POBJECT_DIRECTORY ObpRootDirectory;

BSTATUS ObiAllocateObject(
	POBJECT_TYPE Type,
	const char* Name,
	size_t BodySize,
	bool NonPaged,
	void* ParentDirectory,
	void* ParseContext,
	int Flags,
	POBJECT_HEADER* OutObjectHeader
);

BSTATUS ObiCreateObjectType(
	const char* TypeName,
	POBJECT_TYPE_INFO TypeInfo,
	POBJECT_TYPE* OutObjectType
);

// note: Doesn't lock the directory mutex, so caller must!
BSTATUS ObiCreateObject(
	void** OutObject,
	POBJECT_DIRECTORY ParentDirectory,
	POBJECT_TYPE ObjectType,
	const char* Name,
	int Flags,
	bool NonPaged,
	void* ParseContext,
	size_t BodySize
);

BSTATUS ObiCreateDirectoryObject(
	POBJECT_DIRECTORY* OutDirectory,
	POBJECT_DIRECTORY ParentDirectory,
	const char* Name,
	int Flags
);

void ObiDereferenceByPointerObject(void* Object);

#ifdef DEBUG
BSTATUS ObiDebugObject(void* Object);
void ObpDebugRootDirectory();
#endif

// Actually private
void ObpInitializeObjectTypeInfos();

void ObpInitializeObjectTypeTypeInfo();
void ObpInitializeDirectoryTypeInfo();
void ObpInitializeSymbolicLinkTypeInfo();

void ObpAddObjectToDirectory(POBJECT_DIRECTORY Directory, POBJECT_HEADER Header);
void ObpRemoveObjectFromDirectory(POBJECT_DIRECTORY Directory, POBJECT_HEADER Header);

void ObpEnterDirectoryMutex();
void ObpExitDirectoryMutex();
void ObpInitializeRootDirectory();

// Note: this remove function doesn't actually remove the object -
// it'll still hang around!
void ObpAddReferenceToObject(void* Object);
void ObpRemoveReferenceFromObject(void* Object);

// Check if a provided name is invalid.
bool ObpCheckNameInvalid(const char* Name, int Flags);

#define OBP_CHECK_BACKSLASHES (1)
