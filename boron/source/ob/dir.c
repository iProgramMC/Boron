/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/dir.c
	
Abstract:
	This module implements the directory object type.
	
Author:
	iProgramInCpp - 27 November 2023
***/
#include "obp.h"

BSTATUS ObCreateDirectoryObject(
	POBJECT_DIRECTORY* DirectoryOut,
	POBJECT_ATTRIBUTES ObjectAttributes)
{
	POBJECT_DIRECTORY Directory;
	BSTATUS Status;
	KPROCESSOR_MODE PreviousMode;
	void* DirectoryAddr;
	
	PreviousMode = KeGetPreviousMode();
	
	// Allocate and initialize the directory object.
	Status = ObCreateObject(
		ObpDirectoryObjectType,
		ObjectAttributes,
		PreviousMode,
		NULL,
		sizeof (*Directory),
		&DirectoryAddr
	);
	
	Directory = DirectoryAddr;
	
	if (Status)
		return Status;
	
	memset(Directory, 0, sizeof *Directory);
	
	KeInitializeMutex(&Directory->Mutex, OBJ_LEVEL_DIRECTORY);
	
	*DirectoryOut = Directory;
	
	return STATUS_SUCCESS;
}

typedef struct OBP_LIST_DIR_CONTEXT_tag
{
	PBSTATUS StatusPtr;
	POBJECT_DIRECTORY Directory;
	POBJECT_DIRENT Buffer;
	size_t MaxBufferCount;
	size_t EntryCount;
}
OBP_LIST_DIR_CONTEXT, *POBP_LIST_DIR_CONTEXT;

static bool ObListDirectorySub(void* ContextV, PAATREE_ENTRY Entry)
{
	POBP_LIST_DIR_CONTEXT Context = ContextV;
	
	if (Context->EntryCount == Context->MaxBufferCount)
	{
	ReQuery:
		*Context->StatusPtr = STATUS_REQUERY;
		return false;
	}
	
	POBJECT_DIRECTORY_ENTRY DirEntry = CONTAINING_RECORD(Entry, OBJECT_DIRECTORY_ENTRY, Entry);
	POBJECT_HEADER Object = OBJECT_GET_HEADER(DirEntry->Object);
	
	POBJECT_DIRENT DirEnt = &Context->Buffer[Context->EntryCount++];
	DirEnt->Name = Object->ObjectName;
	DirEnt->TypeName = OBJECT_GET_HEADER(Object->NonPagedHeader->Type)->ObjectName;
	
	if (Context->EntryCount == Context->MaxBufferCount)
		goto ReQuery;
	
	return true;
}

BSTATUS ObListDirectoryObject(
	POBJECT_DIRECTORY Directory,
	POBJECT_DIRENT Buffer,
	size_t MaxBufferCount,
	size_t* EntryCountOut)
{
	BSTATUS Status = STATUS_SUCCESS; // Optimistic!
	KeWaitForSingleObject(&Directory->Mutex, false, TIMEOUT_INFINITE);
	
	OBP_LIST_DIR_CONTEXT Context;
	Context.StatusPtr = &Status;
	Context.Directory = Directory;
	Context.Buffer = Buffer;
	Context.MaxBufferCount = MaxBufferCount;
	Context.EntryCount = 0;
	
	ExTraverseInOrderAaTree(&Directory->Tree, ObListDirectorySub, &Context);
	
	*EntryCountOut = Context.EntryCount;
	
	KeReleaseMutex(&Directory->Mutex);
	return Status;
}

static AATREE_KEY ObpComputeTreeKey(const char* Name, void* Pointer)
{
	// This is horrible, but it should get the job done.
	if (sizeof(AATREE_KEY) == 8)
	{
		union
		{
			struct
			{
				char Part[4];
				char Prat[4];
			} y;
			AATREE_KEY Key;
		} x;
			
		x.y.Part[0] = Name[0];
		x.y.Part[1] = x.y.Part[2] = x.y.Part[3] = 0;
		if (x.y.Part[0])
		{
			x.y.Part[1] = Name[1];
			if (x.y.Part[1])
			{
				x.y.Part[2] = Name[2];
				if (x.y.Part[2])
					x.y.Part[3] = Name[3];
			}
		}
		
		x.y.Prat[0] = (char)((uintptr_t)Pointer);
		x.y.Prat[1] = (char)((uintptr_t)Pointer >> 8);
		x.y.Prat[2] = (char)((uintptr_t)Pointer >> 16);
		x.y.Prat[3] = (char)((uintptr_t)Pointer >> 24);
		
		return x.Key;
	}
	else
	{
		union
		{
			struct
			{
				char Part[2];
				char Prat[2];
			} y;
			AATREE_KEY Key;
		} x;
			
		x.y.Part[0] = Name[0];
		x.y.Part[1] = 0;
		if (x.y.Part[0])
			x.y.Part[1] = Name[1];
		
		x.y.Prat[0] = (char)((uintptr_t)Pointer);
		x.y.Prat[1] = (char)((uintptr_t)Pointer >> 8);
		
		return x.Key;
	}
}

// Parameters:
//  MaxBufferCount - The maximum amount of objects that can be stored in Buffer.
//  EntryCountOut  - The number of directory entries that exist.  Must not be NULL.
// Returns STATUS_REQUERY if MaxBufferCount < *EntryCountOut
BSTATUS ObpInsertObjectIntoDirectory(
	POBJECT_DIRECTORY Directory,
	void* Object)
{
	POBJECT_HEADER Header = OBJECT_GET_HEADER(Object);
	
	KeWaitForSingleObject(&Directory->Mutex, false, TIMEOUT_INFINITE);
	
	POBJECT_DIRECTORY_ENTRY DirEntry = MmAllocatePool(POOL_NONPAGED, sizeof(OBJECT_DIRECTORY_ENTRY));
	DirEntry->Object = Object;
	DirEntry->Entry.Key = ObpComputeTreeKey(Header->ObjectName, Directory);
	
	int Iterations = 0;
Retry:
	if (!ExInsertItemAaTree(&Directory->Tree, &DirEntry->Entry))
	{
		// There is a hash collision!
		// Look for the item that was there - TODO: make the AA tree do that for us
		PAATREE_ENTRY CollidedEntry = ExLookUpItemAaTree(&Directory->Tree, DirEntry->Entry.Key);
		if (!CollidedEntry)
			// Huh?!
			goto Retry;
		
		POBJECT_DIRECTORY_ENTRY CollidedDirEntry = CONTAINING_RECORD(CollidedEntry, OBJECT_DIRECTORY_ENTRY, Entry);
		POBJECT_HEADER ObjHdr = OBJECT_GET_HEADER(CollidedDirEntry->Object);
		if (strcmp(ObjHdr->ObjectName, Header->ObjectName) == 0)
		{
			// Already added!!
			MmFreePool(DirEntry);
			KeReleaseMutex(&Directory->Mutex);
			return STATUS_NAME_COLLISION;
		}
		
		// Increment the key by 1<<(keysize/2), and try again.
		if (sizeof(AATREE_KEY) == 8)
			DirEntry->Entry.Key += 0x100000000ULL;
		else
			DirEntry->Entry.Key += 0x10000;
		
		if (++Iterations != 100000) // Arbitrary
			goto Retry;
		
		KeCrash("ObpInsertObjectIntoDirectory: huh");
	}
	
	KeReleaseMutex(&Directory->Mutex);
	return STATUS_SUCCESS;
}
