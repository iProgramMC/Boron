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

// Pointer to root directory.
POBJECT_DIRECTORY ObpRootDirectory;
POBJECT_DIRECTORY ObpObjectTypesDirectory;

bool ObpInitializedRootDirectory()
{
	return ObpRootDirectory != NULL;
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

// Matches a file name with the first segment of the path name,
// up until a backslash ('\') character.
// Returns 0 if not matched, >0 (length of path consumed) if matched.
static size_t ObpMatchPathName(const char* FileName, const char* Path)
{
	const char* OriginalPath = Path;
	
	// While the file name and path are the same up to this point...
	//
	// N.B. If the file name reached the '\0' at this point, then either
	// the Filename==Path condition or the Path!=0 condition are false.
	while (*FileName == *Path && *Path != '\0' && *Path != OB_PATH_SEPARATOR)
	{
		FileName++;
		Path++;
	}
	
	// Check if the file name and path reached the end together:
	if (*FileName == '\0' && (*Path == '\0' || *Path == OB_PATH_SEPARATOR))
	{
		return Path - OriginalPath + (*Path == OB_PATH_SEPARATOR);
	}
	
	return 0;
}

BSTATUS ObpParseDirectory(
	void* ParseObject,
	const char** Name,
	void* Context UNUSED,
	void** Object)
{
	POBJECT_DIRECTORY Directory = ParseObject;
	
	const char* CompleteName = *Name;
	
	// If the first character in the name is a backslash, simply
	// ignore it and request continuing the parse procedure. The
	// path could have been malformed, something like this:
	// \Something\\\AnotherThing
	if (*CompleteName == OB_PATH_SEPARATOR)
	{
		*Name = CompleteName + 1;
		*Object = ParseObject;
		return STATUS_SUCCESS;
	}
	
	PLIST_ENTRY Entry = Directory->ListHead.Flink;
	while (Entry != &Directory->ListHead)
	{
		POBJECT_HEADER Header = CONTAINING_RECORD(Entry, OBJECT_HEADER, DirectoryListEntry);
		
		size_t MatchLength = ObpMatchPathName(Header->ObjectName, CompleteName);
		if (MatchLength > 0)
		{
			// Match! Time to return this object.
			*Object = Header->Body;
			
			// Add a reference of course
			Header->NonPagedObjectHeader->PointerCount++;
			
			*Name = CompleteName + MatchLength;
			
			return STATUS_SUCCESS;
		}
		
		Entry = Entry->Flink;
	}
	
	*Object = NULL;
	return STATUS_NAME_NOT_FOUND;
}

void ObpListDirectory(POBJECT_DIRECTORY Directory)
{
	DbgPrint("Listing of directory %s", OBJECT_GET_HEADER(Directory)->ObjectName);
	
	POBJECT_HEADER Entry = NULL;
	while (true)
	{
		BSTATUS Status = ObpGetNextEntryDirectory(Directory, Entry, &Entry);
		if (Status != STATUS_SUCCESS)
		{
			if (Status != STATUS_DIRECTORY_DONE)
				DbgPrint("Error, status is %d", Status);
			
			break;
		}
		
		DbgPrint("\t%-30s %s", Entry->ObjectName, Entry->NonPagedObjectHeader->ObjectType->TypeName);
	}
}

#ifdef DEBUG
void ObpDebugRootDirectory()
{
	ObiDebugObject(ObpRootDirectory);
	ObiDebugObject(ObpObjectTypesDirectory);
	
	const char* Path = "\\ObjectTypes\\Directory";
	void* MatchObject = ObpDirectoryType;
	void* Object = NULL;
	BSTATUS Status = ObiLookUpObject(NULL, Path, &Object);
	
	if (FAILED(Status))
	{
		DbgPrint("Error, lookup failed on %s with status %d", Path, Status);
	}
	else if (Object != MatchObject)
	{
		DbgPrint("Error, object %p doesn't match matchobject %p", Object, MatchObject);
	}
	else
	{
		DbgPrint("Hey, lookup %s works!", Path);
	}
	
	// Try to create a new object
	void* Obj = NULL;
	Status = ObiCreateObject(&Obj, NULL, ObpSymbolicLinkType, "\\ObjectTypes\\MyTestObject", 0, true, NULL, 42);
	if (FAILED(Status))
	{
		DbgPrint("Failed to create object: %d", Status);
	}
	else
	{
		DbgPrint("It worked! Obj: %p", Obj);
		
		// Try to look it up now
		void* Obj2 = NULL;
		Status = ObiLookUpObject(NULL, "\\ObjectTypes\\MyTestObject", &Obj2);
		if (SUCCEEDED(Status))
		{
			DbgPrint("Lookup succeeded! %p was returned", Obj2);
			
			if (Obj != Obj2)
				DbgPrint("Pointers are not the same!!");
		}
		else
		{
			DbgPrint("Lookup failed: %d", Status);
		}
	}
	
	ObiDebugObject(ObpObjectTypesDirectory);
	
	// try the other way of listing directories
	ObpListDirectory(ObpRootDirectory);
	ObpListDirectory(ObpObjectTypesDirectory);
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
	O->Parse = ObpParseDirectory;
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
	
	ObiDereferenceObject(Directory);
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

BSTATUS ObpCopyPathSegment(const char** PathName, char* SegmentOut)
{
	size_t Written = 0;
	const char* Name = *PathName;
	
	while (*Name != '\0' && *Name != OB_PATH_SEPARATOR)
	{
		if (Written >= OB_MAX_PATH_LENGTH - 1)
			return STATUS_NAME_INVALID;
		
		*SegmentOut = *Name;
		SegmentOut++;
		Name++;
		Written++;
	}
	
	// If we ended on a '\0', that means we don't have segments after this
	if (*Name == '\0')
		return STATUS_NO_SEGMENTS_AFTER_THIS;
	
	*PathName = Name + 1; // Also skip the backslash
	*SegmentOut = '\0';
	return STATUS_SUCCESS;
}

BSTATUS ObpNormalizeParentDirectoryAndName(
	POBJECT_DIRECTORY* ParentDirectory,
	const char** ObjectName)
{
	if (!*ParentDirectory)
	{
		*ParentDirectory = ObpRootDirectory;
		if (**ObjectName != OB_PATH_SEPARATOR)
		{
			// Relative path with no parent directory.
			return STATUS_PATH_INVALID;
		}
		
		// Skip the path separator.
		(*ObjectName)++;
		
		DbgPrint("ObpNormalizeParentDirectoryAndName: No parent directory, obj name: %s", *ObjectName);
	}
	
	char Segment[OB_MAX_PATH_LENGTH];
	
	BSTATUS Status;
	while (SUCCEEDED(Status = ObpCopyPathSegment(ObjectName, Segment)))
	{
		// Path probably contains neighboring \\ characters, skip
		if (Segment[0] == 0)
			continue;
		
		DbgPrint("ObpNormalizeParentDirectoryAndName: %s, %s", Segment, *ObjectName);
		
		void* NewObject = NULL;
		Status = ObiLookUpObject(*ParentDirectory, Segment, &NewObject);
		
		if (FAILED(Status))
			return Status;
		
		// Ensure this is a directory.
		if (OBJECT_GET_HEADER(NewObject)->NonPagedObjectHeader->ObjectType != ObpDirectoryType)
			return STATUS_PATH_INVALID;
		
		// Advance.
		*ParentDirectory = (POBJECT_DIRECTORY)NewObject;
	}
	
	if (Status == STATUS_NO_SEGMENTS_AFTER_THIS)
		Status =  STATUS_SUCCESS;
	
	return Status;
}

BSTATUS ObpGetNextEntryDirectory(
	POBJECT_DIRECTORY Directory,
	POBJECT_HEADER InEntry,
	POBJECT_HEADER* OutEntry)
{
	if (InEntry == NULL)
	{
		// InEntry is null, that means they just started listing the directory.
		if (IsListEmpty(&Directory->ListHead))
			return STATUS_DIRECTORY_DONE;
		
		*OutEntry = CONTAINING_RECORD(Directory->ListHead.Flink, OBJECT_HEADER, DirectoryListEntry);
		return STATUS_SUCCESS;
	}
	
	PLIST_ENTRY Entry = &InEntry->DirectoryListEntry;
	if (Entry->Flink == &Directory->ListHead)
	{
		// Directory listing has concluded
		return STATUS_DIRECTORY_DONE;
	}
	
	*OutEntry = CONTAINING_RECORD(Entry->Flink, OBJECT_HEADER, DirectoryListEntry);
	return STATUS_SUCCESS;
}
