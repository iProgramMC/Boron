/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/link.c
	
Abstract:
	This module implements the symbolic link object type for
	the object manager.
	
Author:
	iProgramInCpp - 23 December 2023
***/
#include "obp.h"

BSTATUS ObpParseSymbolicLink(
	void* SymLinkObject,
	UNUSED const char** Name,
	//char* TemporarySpace,
	UNUSED void* Context,
	int LoopCount,
	void** OutObject)
{
	POBJECT_SYMLINK SymbolicLink = SymLinkObject;
	
	if (!SymbolicLink->DestPath)
		// The symbolic link was not assigned.
		return STATUS_UNASSIGNED_LINK;
	
	// Perform a lookup on the destination path.
	BSTATUS Status = ObpLookUpObjectPath(
		NULL,
		SymbolicLink->DestPath,
		NULL,
		LoopCount,
		0,
		OutObject
	);
	
	// That's all, folks!
	//
	// N.B. We are only human, after all, so symbolic link lookups can fail.
	// Not a problem, OutObject won't have been overwritten by a valid pointer
	// if we failed.
	//
	// N.B. #2: We don't touch the remaining name.
	// Suppose we try to access the path: \Boron\Crap\LINK\Something, and the
	// LINK component represents a symbolic link to \LinkMeHere. The lookup
	// routine enters the directory \Boron, and then \Boron\Crap. Then it finds
	// our symbolic link, with \Something left to parse. Essentially, this function
	// performs a really neat trick which swaps the directory from under the rug of
	// the lookup routine, so suddenly, it's not in \Boron\Crap, it's in \LinkMeHere.
	// Afterwards, it finds the correct object \LinkMeHere\Something.
	return Status;
}

void ObpDeleteSymbolicLink(void* SymLinkVoid)
{
	POBJECT_SYMLINK SymLink = SymLinkVoid;
	
	if (SymLink->DestPath)
		MmFreePool(SymLink->DestPath);
	
	SymLink->DestPath = NULL;
}

extern POBJECT_TYPE ObSymbolicLinkType;

OBJECT_TYPE_INFO ObpSymbolicLinkTypeInfo =
{
	// InvalidAttributes
	0,
	// ValidAccessMask
	0,
	// NonPagedPool
	true,
	// MaintainHandleCount
	false,
	// Open
	NULL,
	// Close
	NULL,
	// Delete
	ObpDeleteSymbolicLink,
	// Parse
	ObpParseSymbolicLink,
	// Secure
	NULL,
	// Duplicate
	NULL,
#ifdef DEBUG
	// Debug
	NULL,
#endif
};

BSTATUS ObpInitializeSymbolicLink(void* Object, void* Context)
{
	POBJECT_SYMLINK SymLink = Object;
	const char* LinkTarget = Context;
	
	size_t Length = strlen(LinkTarget);
	
	// Fill in the symbolic link.
	SymLink->DestPath = MmAllocatePool(POOL_FLAG_NON_PAGED, Length + 1);
	if (!SymLink->DestPath)
		return STATUS_INSUFFICIENT_MEMORY;
	
	strcpy(SymLink->DestPath, LinkTarget);
	return STATUS_SUCCESS;
}

BSTATUS ObCreateSymbolicLinkObject(
	void** OutObject,
	const char* LinkTarget,
	const char* ObjectName,
	int Flags)
{
	BSTATUS Status;
	
	void* Object;
	
	// Attempt to resolve the link target first.  A symbolic link
	// may only be created if the object actually exists.
	Status = ObpLookUpObjectPath(
		NULL,       // InitialParseObject
		LinkTarget, // ObjectName
		NULL,       // ExpectedType
		0,          // LoopCount
		0,          // OpenFlags
		&Object     // FoundObject
	);
	
	if (FAILED(Status))
	{
		// Object wasn't found.
		return Status;
	}
	
	// That's all we wanted to do, just check for the object's existence.
	ObDereferenceObject(Object);
	Object = NULL;
	
	// Create the link itself.
	Status = ObCreateObjectCallback(
		&Object,
		NULL, // ParentDirectory
		ObSymbolicLinkType,
		ObjectName,
		Flags | OB_FLAG_NONPAGED, // <--- TODO: Right now, everything related to object directories is nonpaged.
		NULL,
		sizeof(OBJECT_SYMLINK),
		ObpInitializeSymbolicLink,
		(void*) LinkTarget
	);
	
	if (FAILED(Status))
		return Status;
	
	*OutObject = Object;
	
	// Ta-da!
	return STATUS_SUCCESS;
}
	
