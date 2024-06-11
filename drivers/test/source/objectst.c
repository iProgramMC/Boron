/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	objectst.c
	
Abstract:
	This module, part of the test driver, implements the
	Object Manager test.
	
Author:
	iProgramInCpp - 11 June 2024
***/
#include <ob.h>
#include "tests.h"

POBJECT_TYPE TstDirectoryType, TstSymbolicLinkType, TstObjectTypeType;

static void AttemptFetchObjectTypes()
{
	BSTATUS Status;
	
	void* Object = NULL;
	Status = ObReferenceObjectByName("\\ObjectTypes\\Type", NULL, 0, NULL, &Object);
	if (FAILED(Status))
	{
		LogMsg("ERROR: Object test failed with error %d. Could not locate \\ObjectTypes\\Type", Status);
		ASSERT(false);
	}
	
	TstObjectTypeType = Object;
	
	Status = ObReferenceObjectByName("\\ObjectTypes\\SymbolicLink", NULL, 0, TstObjectTypeType, &Object);
	if (FAILED(Status))
	{
		LogMsg("ERROR: Object test failed with error %d. Could not locate \\ObjectTypes\\SymbolicLink", Status);
		ASSERT(false);
	}
	
	TstSymbolicLinkType = Object;
	
	Status = ObReferenceObjectByName("\\ObjectTypes\\SymbolicLink", NULL, 0, TstObjectTypeType, &Object);
	if (FAILED(Status))
	{
		LogMsg("ERROR: Object test failed with error %d. Could not locate \\ObjectTypes\\DirectoryType", Status);
		ASSERT(false);
	}
	
	TstDirectoryType = Object;
}

static void AttemptCreateSymLink()
{
	BSTATUS Status;
	
	// Try creating a symbolic link:
	void *Symlink = NULL;
	Status = ObCreateSymbolicLinkObject(
		&Symlink,
		"\\ObjectTypes",
		"\\ObjectTypes\\Test Link",
		OB_FLAG_KERNEL
	);
	
	if (FAILED(Status))
	{
		LogMsg("ERROR: Can't create symlink: %d", Status);
		ASSERT(false);
		return;
	}
	
	const char* TestPath = "\\ObjectTypes\\Test Link\\Test Link\\Test Link\\Directory";
	
	// Try looking up a certain path:
	void* Obj = NULL;
	Status = ObReferenceObjectByName(
		TestPath,
		NULL,              // InitialParseObject
		0,                 // OpenFlags
		TstObjectTypeType, // ExpectedType
		&Obj
	);
	
	if (FAILED(Status))
	{
		LogMsg("ERROR: Look up of path '%s' failed with error: %d", TestPath, Status);
	}
	else
	{
		LogMsg("Look up of path '%s' succeeded.", TestPath);
			
		LogMsg("It returned object %p with type %p", Obj, OBJECT_GET_HEADER(Obj)->NonPagedObjectHeader->ObjectType);
		
		LogMsg(
			"Init types: ObpObjectTypeType: %p, ObpDirectoryType: %p, ObpSymbolicLinkType: %p)",
			TstObjectTypeType,
			TstDirectoryType,
			TstSymbolicLinkType
		);
	}
}

void PerformObjectTest()
{
	AttemptFetchObjectTypes();
	AttemptCreateSymLink();
}
