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

// Actually private
void ObpInitializeObjectTypeInfos();
