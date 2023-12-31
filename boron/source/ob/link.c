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

extern POBJECT_TYPE ObpSymbolicLinkType;

OBJECT_TYPE_INFO ObpSymbolicLinkTypeInfo =
{
	// InvalidAttributes
	0,
	// ValidAccessMask
	0,
	// NonPagedPool
	true,
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
#ifdef DEBUG
	// Debug
	NULL,
#endif
};

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
		&Object
	);
	
	if (FAILED(Status))
	{
		// Object wasn't found.
		return Status;
	}
	
	// That's all we wanted to do, just check for the object's existence.
	ObpDereferenceObject(Object);
	
	ObpEnterRootDirectoryMutex();
	
	// Create the link itself.
	Status = ObCreateObject(
		&Object,
		NULL, // ParentDirectory
		ObpSymbolicLinkType,
		ObjectName,
		Flags,
		false, // NonPaged
		NULL,
		sizeof(OBJECT_SYMLINK)
	);
	
	if (FAILED(Status))
	{
		ObpLeaveRootDirectoryMutex();
		return Status;
	}
	
	size_t Length = strlen(LinkTarget);
	
	// Fill in the symbolic link.
	POBJECT_SYMLINK SymLink = Object;
	
	SymLink->DestPath = MmAllocatePool(POOL_FLAG_NON_PAGED, Length + 1);
	strcpy(SymLink->DestPath, LinkTarget);
	
	*OutObject = Object;
	
	ObpLeaveRootDirectoryMutex();
	
	// Ta-da!
	return STATUS_SUCCESS;
}
	
