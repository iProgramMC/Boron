/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/dir.c
	
Abstract:
	This module implements the "Directory" object type. It
	provides create directory, .. functions
	
Author:
	iProgramInCpp - 8 December 2023
***/
#include "obp.h"

OBJECT_TYPE_INFO ObpDirectoryTypeInfo;

// Directory structure mutex.
KMUTEX ObpDirectoryMutex;

// Pointer to root directory.
POBJECT_DIRECTORY ObpRootDirectory;
POBJECT_DIRECTORY ObpObjectTypesDirectory;

void ObpEnterDirectoryMutex()
{
	KeWaitForSingleObject(&ObpDirectoryMutex, false, TIMEOUT_INFINITE);
}

void ObpExitDirectoryMutex()
{
	KeReleaseMutex(&ObpDirectoryMutex);
}


void ObpInitializeRootDirectory()
{
	if (FAILED(ObiCreateDirectoryObject(
		&ObpRootDirectory,
		NULL,
		"Root",
		OB_FLAG_KERNEL | OB_FLAG_PERMANENT
	)))
	{
		KeCrash("cannot create root directory");
	}
	
	// Create the object types directory.
	if (FAILED(ObiCreateDirectoryObject(
		&ObpObjectTypesDirectory,
		ObpRootDirectory,
		"ObjectTypes",
		OB_FLAG_KERNEL | OB_FLAG_PERMANENT
	)))
	{
		KeCrash("cannot create ObjectTypes directory");
	}
	
	// Add all object types created so far to the object types directory.
	ObpAddObjectToDirectory(ObpObjectTypesDirectory, OBJECT_GET_HEADER(ObpObjectTypeType));
	ObpAddObjectToDirectory(ObpObjectTypesDirectory, OBJECT_GET_HEADER(ObpDirectoryType));
	ObpAddObjectToDirectory(ObpObjectTypesDirectory, OBJECT_GET_HEADER(ObpSymbolicLinkType));
	
#ifdef DEBUG
	ObpDebugRootDirectory();
#endif
}

#ifdef DEBUG
void ObpDebugRootDirectory()
{
	ObiDebugObject(ObpRootDirectory);
	ObiDebugObject(ObpObjectTypesDirectory);
}
#endif

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

void ObpInitializeDirectoryTypeInfo()
{
	POBJECT_TYPE_INFO O = &ObpDirectoryTypeInfo;
	
	O->NonPagedPool = true;
	
	#ifdef DEBUG
	O->Debug = ObpDebugDirectory;
	#endif
}

BSTATUS ObiCreateDirectoryObject(
	POBJECT_DIRECTORY* OutDirectory,
	POBJECT_DIRECTORY ParentDirectory,
	const char* Name,
	int Flags)
{
	POBJECT_DIRECTORY NewDir;
	BSTATUS Status;
	
	Status = ObiCreateObject(
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
		return Status;
	
	// Initialize the directory object.
	InitializeListHead(&NewDir->ListHead);
	NewDir->Count = 0;
	
	*OutDirectory = NewDir;
	return STATUS_SUCCESS;
}

void ObpAddObjectToDirectory(POBJECT_DIRECTORY Directory, POBJECT_HEADER Header)
{
	InsertTailList(&Directory->ListHead, &Header->DirectoryListEntry);
	Directory->Count++;
	ObpAddReferenceToObject(Directory);
}

void ObpRemoveObjectFromDirectory(POBJECT_DIRECTORY Directory, POBJECT_HEADER Header)
{
	RemoveEntryList(&Header->DirectoryListEntry);
	Directory->Count--;
	
	ObDereferenceObject(Directory);
}

bool ObpCheckNameInvalid(const char* Name, int Flags)
{
	while (*Name)
	{
		if (*Name < ' ' || *Name > '~')
			return false;
		
		if ((Flags & OBP_CHECK_BACKSLASHES) && *Name == OB_PATH_SEPARATOR)
			return false;
		
		Name++;
	}
	
	return true;
}

