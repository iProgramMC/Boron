/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/init.c
	
Abstract:
	This module implements the initialization code for the
	object manager.
	
Author:
	iProgramInCpp - 7 December 2023
***/
#include "obp.h"

extern OBJECT_TYPE_INFO ObpObjectTypeTypeInfo;
extern OBJECT_TYPE_INFO ObpDirectoryTypeInfo;
extern OBJECT_TYPE_INFO ObpSymbolicLinkTypeInfo;

// for testing
BSTATUS ObpDebugObjectType(void* Object);

void ObInitializeFirstPhase()
{
	ObpInitializeObjectTypeInfos();
	
	// Create the ObjectType type.
	if (FAILED(ObiCreateObjectType("ObjectType", &ObpObjectTypeTypeInfo, &ObpObjectTypeType)))
		KeCrash("could not create ObjectType object type");
	
	if (FAILED(ObiCreateObjectType("Directory", &ObpDirectoryTypeInfo, &ObpDirectoryType)))
		KeCrash("could not create Directory object type");
	
	if (FAILED(ObiCreateObjectType("SymbolicLink", &ObpSymbolicLinkTypeInfo, &ObpSymbolicLinkType)))
		KeCrash("could not create SymbolicLink object type");
	
	ObpDebugObjectType(ObpObjectTypeType);
	ObpDebugObjectType(ObpDirectoryType);
	ObpDebugObjectType(ObpSymbolicLinkType);
}

void ObInitializeSecondPhase()
{
}
