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
extern POBJECT_TYPE ObpSymbolicLinkType;

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

BSTATUS ObLinkObject(
	POBJECT_DIRECTORY Directory,
	void* Object)
{
	ObpEnterRootDirectoryMutex();
	
	// Add it proper
	InsertTailList(&Directory->ListHead, &OBJECT_GET_HEADER(Object)->DirectoryListEntry);
	Directory->Count++;
	ObReferenceObjectByPointer(Directory);
	
	POBJECT_HEADER Header = OBJECT_GET_HEADER(Object);
	Header->ParentDirectory = Directory;
	
	ObpLeaveRootDirectoryMutex();
	
	return STATUS_SUCCESS;
}

// Same as ObpRemoveObjectFromDirectory, but with the root directory mutex entered
void ObpRemoveObjectFromDirectoryLocked(POBJECT_DIRECTORY Directory, void* Object)
{
	POBJECT_HEADER Header = OBJECT_GET_HEADER(Object);
	
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
	ObDereferenceObject(Directory);
}

void ObpRemoveObjectFromDirectory(POBJECT_DIRECTORY Directory, void* Object)
{
	ObpEnterRootDirectoryMutex();
	ObpRemoveObjectFromDirectoryLocked (Directory, Object);
	ObpLeaveRootDirectoryMutex();
}

// Matches a file name with the first segment of the path name,
// up until a backslash ('\') character.
// Returns 0 if not matched, >0 (length of path consumed) if matched.
//
// A segment from a path name, also known as component of a path name,
// is the name of a directory entry that is entered during path traversal.
// For example, in the path name \ObjectTypes\Directory, ObjectTypes and
// Directory are two separate segments of the path name.
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
//
// N.B. The only open flag checked is "OB_OPEN_SYMLINK".
BSTATUS ObpLookUpObjectPath(
	void* InitialParseObject,
	const char* ObjectName,
	POBJECT_TYPE ExpectedType,
	int LoopCount,
	int OpenFlags,
	void** FoundObject)
{
	if (!ObjectName || !FoundObject)
		return STATUS_INVALID_PARAMETER;
	
	if (OB_MAX_LOOP <= LoopCount)
		// We can't risk it!
		return STATUS_LOOP_TOO_DEEP;
	
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
		
		// Check if the parse object is actually a directory.
		if (ObpGetObjectType(InitialParseObject) != ObpDirectoryType)
		{
			ObpLeaveRootDirectoryMutex();
			return STATUS_PATH_INVALID;
		}
	}
	
	ObReferenceObjectByPointer(InitialParseObject);
	
	//char TemporarySpace[OB_MAX_PATH_LENGTH];
	void* CurrentObject = InitialParseObject;
	const char* CurrentPath = ObjectName;
	
	int CurrDepth = 100; // XXX Completely arbitrary
	
	while (CurrDepth > 0)
	{
		// If the object is a directory:
		POBJECT_TYPE ObjectType = ObpGetObjectType(CurrentObject);
		if (ObjectType == ObpDirectoryType)
		{
			// Check if we have any more path to parse.
			if (*CurrentPath == 0)
			{
				// Nope, so return success.
				//
				// N.B. The object is already referenced.
				*FoundObject = CurrentObject;
				ObpLeaveRootDirectoryMutex();
				return STATUS_SUCCESS;
			}
			
			// We do, so browse the directory to find an entry with the same name
			// as the current path segment.
			POBJECT_DIRECTORY Directory = CurrentObject;
			PLIST_ENTRY Entry = Directory->ListHead.Flink;
			
			bool FoundMatch = false;
			
			while (Entry != &Directory->ListHead)
			{
				POBJECT_HEADER Header = CONTAINING_RECORD(Entry, OBJECT_HEADER, DirectoryListEntry);
				
				size_t MatchLength = ObpMatchPathName(Header->ObjectName, CurrentPath);
				if (MatchLength != 0)
				{
					// Match! Time to continue parsing through this object.
					CurrentPath += MatchLength;
					
					ObReferenceObjectByPointer(Header->Body);
					ObDereferenceObject(CurrentObject);
					CurrentObject = Header->Body;
					FoundMatch = true;
					break;
				}
				
				// Didn't match, so continue.
				Entry = Entry->Flink;
			}
			
			if (!FoundMatch)
			{
				// No match! Inform caller about our failure.
				//
				// N.B. We added a reference to the object we were using!
				ObDereferenceObject(CurrentObject);
				ObpLeaveRootDirectoryMutex();
				return STATUS_NAME_NOT_FOUND;
			}
			
			CurrDepth--;
			continue;
		}
		
		if (ObjectType == ObpSymbolicLinkType)
		{
			// Check if this is the last path component, and if the OB_OPEN_SYMLINK flag was specified.
			if (*CurrentPath == 0 && (OpenFlags & OB_OPEN_SYMLINK))
			{
				DbgPrint("Finish parsing because OB_OPEN_SYMLINK was specified");
				goto TerminateParsing;
			}
		}
		
		// If the object can be parsed, try to parse it!
		if (ObjectType->TypeInfo.Parse)
		{
			void* NewObject = NULL;
			BSTATUS Status = ObjectType->TypeInfo.Parse(
				CurrentObject,
				&CurrentPath,
				//TemporarySpace,
				OBJECT_GET_HEADER(CurrentObject)->ParseContext,
				LoopCount + 1,
				&NewObject
			);
			
			if (FAILED(Status))
			{
				ObDereferenceObject(CurrentObject);
				ObpLeaveRootDirectoryMutex();
				return Status;
			}
			
			ObDereferenceObject(CurrentObject);
			ObDereferenceObject(NewObject);
			CurrentObject = NewObject;
			
			CurrDepth--;
			continue;
		}
		
		// The object can't be parsed, which means this is a dead end!
		if (*CurrentPath != '\0')
		{
			// There's still more path to be parsed!
			// This means the path is like \ObjectTypes\Directory\CantFindMe.
			// Obviously \ObjectTypes\Directory isn't a directory or symbolic link
			// we can parse, so CantFindMe can't possibly exist.
			ObDereferenceObject(CurrentObject);
			ObpLeaveRootDirectoryMutex();
			return STATUS_PATH_INVALID;
		}
		
	TerminateParsing:
		// Ok, this is the object, let's check if its type matches.
		if (ExpectedType != NULL && ExpectedType != ObjectType)
		{
			ObDereferenceObject(CurrentObject);
			ObpLeaveRootDirectoryMutex();
			return STATUS_TYPE_MISMATCH;
		}
		
		*FoundObject = CurrentObject;
		ObpLeaveRootDirectoryMutex();
		return STATUS_SUCCESS;
	}
	
	// There shouldn't be another way to break out of this loop.
	ASSERT(CurrDepth == 0);
	
	return STATUS_PATH_TOO_DEEP;
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
		Flags | OB_FLAG_NONPAGED,
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
	// MaintainHandleCount
	false,
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
	if (FAILED(ObLinkObject(ObpObjectTypesDirectory, ObpObjectTypeType))) return false;
	if (FAILED(ObLinkObject(ObpObjectTypesDirectory, ObpDirectoryType))) return false;
	if (FAILED(ObLinkObject(ObpObjectTypesDirectory, ObpSymbolicLinkType))) return false;
	
	ObpDebugDirectory(ObpRootDirectory);
	ObpDebugDirectory(ObpObjectTypesDirectory);
	
	return true;
}

BSTATUS ObpNormalizeParentDirectoryAndName(
	POBJECT_DIRECTORY* ParentDirectory,
	const char** Name)
{
	// If:
	// - we don't have a pointer to the parent directory pointer
	// - we don't have a pointer to a string
	// - we don't have the string
	// - we don't have anything within the string
	// then we bail out because parameters are invalid.
	if (!ParentDirectory || !Name || !*Name || !**Name)
		return STATUS_INVALID_PARAMETER;
	
	if (*ParentDirectory == NULL)
	{
		if (**Name != OB_PATH_SEPARATOR)
			return STATUS_INVALID_PARAMETER;
		
		*ParentDirectory = ObpRootDirectory;
		(*Name)++;
	}
	
	if (**Name == OB_PATH_SEPARATOR)
		return STATUS_INVALID_PARAMETER;
	
	size_t PathLength = strlen(*Name);
	if (PathLength >= OB_MAX_PATH_LENGTH - 1)
		return STATUS_NAME_TOO_LONG;
	
	char PathWithoutName[OB_MAX_PATH_LENGTH];
	const char *ActualName = *Name, *FinalName = NULL;
	
	size_t MinIndex = 0;
	MinIndex--;
	bool StartCopying = false;
	PathWithoutName[PathLength] = 0;
	for (size_t Index = PathLength - 1; Index != MinIndex; Index--)
	{
        if (StartCopying)
		{
			PathWithoutName[Index] = ActualName[Index];
		}
		else if (ActualName[Index] == OB_PATH_SEPARATOR)
		{
			StartCopying = true;
			FinalName = ActualName + Index + 1;
			PathWithoutName[Index] = 0;
		}
	}
	
	if (!StartCopying)
	{
		PathWithoutName[0] = 0;
		FinalName = ActualName;
	}
	
	if (*PathWithoutName == '\0')
	{
		// No path to lookup, just return the root directory and the final name.
		*Name = FinalName;
		return STATUS_SUCCESS;
	}
	
	// Look up the containing directory.
	void* ContainingDirectory = NULL;
	BSTATUS Status = ObpLookUpObjectPath(
		*ParentDirectory,
		PathWithoutName,
		ObpDirectoryType,
		0,
		0,
		&ContainingDirectory
	);
	
	if (FAILED(Status))
		return Status;
	
	*ParentDirectory = ContainingDirectory;
	*Name = FinalName;
	return STATUS_SUCCESS;
}

BSTATUS ObUnlinkObject(void* Object)
{
	POBJECT_HEADER Header = OBJECT_GET_HEADER(Object);
	
	ObpEnterRootDirectoryMutex();
	if (!Header->ParentDirectory)
	{
		ObpLeaveRootDirectoryMutex();
		return STATUS_NOT_LINKED;
	}
	
	ObpRemoveObjectFromDirectoryLocked(Header->ParentDirectory, Object);
	ObpLeaveRootDirectoryMutex();
	return STATUS_SUCCESS;
}
