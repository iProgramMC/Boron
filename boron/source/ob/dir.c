/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/dir.c
	
Abstract:
	This module implements the directory object type for
	the object manager.
	
Author:
	iProgramInCpp - 23 December 2023
***/
#include "obp.h"

extern POBJECT_TYPE ObpDirectoryType;

KMUTEX ObpRootDirectoryMutex;

void ObpEnterRootDirectoryMutex()
{
	KeWaitForSingleObject(&ObpRootDirectoryMutex, false, TIMEOUT_INFINITE);
}

void ObpLeaveRootDirectoryMutex()
{
	KeReleaseMutex(&ObpRootDirectoryMutex);
}

POBJECT_DIRECTORY ObpRootDirectory;
POBJECT_DIRECTORY ObpObjectTypesDirectory;

BSTATUS ObpAddObjectToDirectory(
	POBJECT_DIRECTORY Directory,
	void* Object)
{
	ObpEnterRootDirectoryMutex();
	
	// Add it proper
	InsertTailList(&Directory->ListHead, &OBJECT_GET_HEADER(Object)->DirectoryListEntry);
	Directory->Count++;
	ObpReferenceObject(Directory);
	
	ObpLeaveRootDirectoryMutex();
	
	return STATUS_SUCCESS;
}

void ObpRemoveObjectFromDirectory(POBJECT_DIRECTORY Directory, void* Object)
{
	POBJECT_HEADER Header = OBJECT_GET_HEADER(Object);
	
	ObpEnterRootDirectoryMutex();
	
#ifdef DEBUG
	// Assert that this object actually belongs to the directory.
	bool Ok = false;
	PLIST_ENTRY Ent = Directory->ListHead.Flink;
	while (Ent != &Directory->ListHead)
	{
		if (Ent == &Header->DirectoryListEntry)
		{
			Ok = true;
			break;
		}
	}
	
	if (!Ok)
	{
		KeCrash(
			"ObpRemoveObjectFromDirectory: Object %p (%s) isn't actually in directory %p (%s)",
			Object,
			Header->ObjectName,
			Directory,
			OBJECT_GET_HEADER(Directory)->ObjectName
		);
	}
#endif
	
	RemoveEntryList(&Header->DirectoryListEntry);
	Directory->Count--;
	ObpDereferenceObject(Directory);
	
	ObpLeaveRootDirectoryMutex();
}

// Performs a path lookup in the object namespace. It can either be
// an absolute path lookup (from the root of the directory tree), or
// a relative path lookup (from a pre-existing directory).
//
// Note that common links to parent directories do not work. Thus, an
// object with the name of ".." is a totally valid object, one that does
// not necessarily refer to the parent directory. In a future version,
// there may be a ".." symbolic link that points to the parent directory.
//
// N.B. Once returned, the object has an extra reference.
// When done with the object, you must dereference it.
BSTATUS ObpLookUpObjectPath(
	void* InitialParseObject,
	const char* ObjectName,
	POBJECT_TYPE ExpectedType,
	int LoopCount,
	void** OutObject)
{
	if (!ObjectName || !ExpectedType || !OutObject)
		return STATUS_INVALID_PARAMETER;
	
	ObpEnterRootDirectoryMutex();
	
	// If there is no initial parse object, then the path name is absolute
	// and we should start lookup from the root directory. However, we must
	// check if the path is actually absolute; if not, the caller made a
	// mistake.
	if (!InitialParseObject)
	{
		InitialParseObject = ObpRootDirectory;
		
		if (*ObjectName != OB_PATH_SEPARATOR)
		{
			ObpLeaveRootDirectoryMutex();
			return STATUS_PATH_INVALID;
		}
		
		// Skip the separator.
		ObjectName++;
		
		// TODO
	}
	else
	{
		// There is an initial parse object.  Check if the path was actually
		// meant to be absolute. IF yes, return STATUS_PATH_INVALID, as we
		// cannot perform absolute path lookup from an object.
		//
		// N.B. We also check if the object name is empty. 
		if (*ObjectName == OB_PATH_SEPARATOR ||
			*ObjectName == '\0')
		{
			ObpLeaveRootDirectoryMutex();
			return STATUS_PATH_INVALID;
		}
	}
	
	ObpReferenceObject(InitialParseObject);
	
	char TemporarySpace[OB_MAX_PATH_LENGTH];
	
	void* CurrentObject = ObpRootDirectory;
	
	while (true)
	{
		// Get the object's type.
		POBJECT_TYPE Type = OBJECT_GET_HEADER(CurrentObject)->NonPagedObjectHeader->ObjectType;
		
		// If the object is a directory:
		if (Type == ObpDirectoryType)
		{
			// 
		}
	}
}

BSTATUS ObCreateDirectoryObject(
	POBJECT_DIRECTORY* OutDirectory,
	POBJECT_DIRECTORY ParentDirectory,
	const char* Name,
	int Flags)
{
	// Check parameters.
	if (!OutDirectory || !Name)
		return STATUS_INVALID_PARAMETER;
	
	POBJECT_DIRECTORY NewDir;
	BSTATUS Status;
	
	// Acquire the directory mutex, lest we want to use an uninitialized dir.
	// Note that we can enter a mutex more than once on the same thread.
	ObpEnterRootDirectoryMutex();
	
	Status = ObCreateObject(
		(void**) &NewDir,
		ParentDirectory,
		ObpDirectoryType,
		Name,
		Flags,
		true,
		NULL, // TODO
		sizeof(OBJECT_DIRECTORY)
	);
	
	if (FAILED(Status))
	{
		ObpLeaveRootDirectoryMutex();
		return Status;
	}
	
	// Initialize the directory object.
	InitializeListHead(&NewDir->ListHead);
	NewDir->Count = 0;
	
	// Leave the root directory mutex now.
	ObpLeaveRootDirectoryMutex();
	
	*OutDirectory = NewDir;
	return STATUS_SUCCESS;
}

#ifdef DEBUG
BSTATUS ObpDebugDirectory(void* DirP)
{
	POBJECT_DIRECTORY Dir = DirP;
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Dir);
	
	DbgPrint("Directory of '%s'. Number of entries: %d", Hdr->ObjectName, Dir->Count);
	
	PLIST_ENTRY Entry = Dir->ListHead.Flink;
	
	int NumEntries = 0;
	
	while (Entry != &Dir->ListHead)
	{
		POBJECT_HEADER ChildHeader = CONTAINING_RECORD(Entry, OBJECT_HEADER, DirectoryListEntry);
		POBJECT_TYPE ChildType = ChildHeader->NonPagedObjectHeader->ObjectType;
		
		DbgPrint("\t%-20s:\t %s", ChildHeader->ObjectName, OBJECT_GET_HEADER(ChildType)->ObjectName);
		
		NumEntries++;
		
		Entry = Entry->Flink;
	}
	
	// Perform some checking anyways
	ASSERT(NumEntries == Dir->Count);
	
	return STATUS_SUCCESS;
}
#endif

OBJECT_TYPE_INFO ObpDirectoryTypeInfo =
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
	NULL,
	// Parse
	NULL,
	// Secure
	NULL,
#ifdef DEBUG
	// Debug
	ObpDebugDirectory,
#endif
};

bool ObpInitializeRootDirectory()
{
	if (FAILED(ObCreateDirectoryObject(
		&ObpRootDirectory,
		NULL,
		"Root",
		OB_FLAG_KERNEL | OB_FLAG_PERMANENT
	)))
		return false;
	
	// Create the object types directory.
	if (FAILED(ObCreateDirectoryObject(
		&ObpObjectTypesDirectory,
		ObpRootDirectory,
		"ObjectTypes",
		OB_FLAG_KERNEL | OB_FLAG_PERMANENT
	)))
		return false;
	
	extern POBJECT_TYPE ObpObjectTypeType;
	extern POBJECT_TYPE ObpSymbolicLinkType;
	
	// Add all object types created so far to the object types directory.
	if (FAILED(ObpAddObjectToDirectory(ObpObjectTypesDirectory, ObpObjectTypeType))) return false;
	if (FAILED(ObpAddObjectToDirectory(ObpObjectTypesDirectory, ObpDirectoryType))) return false;
	if (FAILED(ObpAddObjectToDirectory(ObpObjectTypesDirectory, ObpSymbolicLinkType))) return false;
	
	ObpDebugDirectory(ObpRootDirectory);
	ObpDebugDirectory(ObpObjectTypesDirectory);
	
	return true;
}
