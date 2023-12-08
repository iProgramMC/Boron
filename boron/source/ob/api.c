/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/api.c
	
Abstract:
	This module implements the kernel mode object manager interface in Boron.
	User mode APIs based on these are exposed in the system call layer, not
	here.
	
Author:
	iProgramInCpp - 8 December 2023
***/
#include "obp.h"

BSTATUS ObCreateObject(
	void** OutObject,
	POBJECT_DIRECTORY Parent,
	POBJECT_TYPE Type,
	const char* Name,
	int Flags,
	bool NonPaged,
	void* ParseContext,
	size_t BodySize)
{
	BSTATUS Status;
	int CheckInvalid = 0;
	
	ObpEnterDirectoryMutex();
	
	// If no parent directory was specified, treat the "Name" as a path.
	if (!Parent)
	{
		/*Status = ObpResolveNameAsPath(&Name, &Parent);
		if (FAILED(Status))
		{
			ObpExitDirectoryMutex();
			return Status;
		}*/
		
		// TODO
		
		Parent = ObpRootDirectory;
		CheckInvalid |= OBP_CHECK_BACKSLASHES;
	}
	else
	{
		// If the parent directory is specified, then the name should not
		// contain any backslashes.
		CheckInvalid |= OBP_CHECK_BACKSLASHES;
	}
	
	if (ObpCheckNameInvalid (Name, CheckInvalid))
	{
		ObpExitDirectoryMutex();
		return STATUS_NAME_INVALID;
	}
	
	
	// Add the actual object itself:
	Status = ObiCreateObject(
		OutObject,
		Parent,
		Type,
		Name,
		Flags,
		NonPaged,
		ParseContext,
		BodySize
	);
	
	ObpExitDirectoryMutex();
	
	return Status;
}
