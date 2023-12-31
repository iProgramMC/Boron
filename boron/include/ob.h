/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob.h
	
Abstract:
	This header file contains the definitions related to
	the Boron object manager.
	
Author:
	iProgramInCpp - 7 December 2023
***/
#pragma once

#include <ke.h>
#include <mm.h>
#include <status.h>

// Lets the object know a handle to it was opened.
typedef void(*OBJ_OPEN_FUNC)  (void* Object, int HandleCount);

// Lets the object know a handle to it was closed.
typedef void(*OBJ_CLOSE_FUNC) (void* Object, int HandleCount);

// Lets the object know that it should tear down because the system will delete it.
typedef void(*OBJ_DELETE_FUNC)(void* Object);

// Parse a path using this object.
//
// N.B.:
//  1. TemporarySpace MUST be OB_MAX_PATH_LENGTH in size!!
//  2. In the implementation of the Parse method, if the Parse method performs a lookup
//     using ObpLookUpObjectPath, then it MUST pass LoopCount into the LoopCount parameter!
//     (I mean, you can always pass LoopCount + 1 if you're stubborn like that, but you will
//     run out of loops twice as quickly if some guy decides they want to create a symlink loop)
typedef BSTATUS(*OBJ_PARSE_FUNC) (
	void* ParseObject,
	const char** Name,
	//char* TemporarySpace,
	void* Context,
	int LoopCount,
	void** Object
);

// Set security properties on this object. TODO
typedef BSTATUS(*OBJ_SECURE_FUNC)(void* Object); // TODO

// Print information about this object on the debug console.
#ifdef DEBUG
typedef BSTATUS(*OBJ_DEBUG_FUNC) (void* Object);
#endif

// Advance type definitions to structures.
typedef struct _OBJECT_TYPE_INFO OBJECT_TYPE_INFO, *POBJECT_TYPE_INFO;
typedef struct _OBJECT_TYPE OBJECT_TYPE, *POBJECT_TYPE;
typedef struct _NONPAGED_OBJECT_HEADER NONPAGED_OBJECT_HEADER, *PNONPAGED_OBJECT_HEADER;
typedef struct _OBJECT_HEADER OBJECT_HEADER, *POBJECT_HEADER;
typedef struct _OBJECT_DIRECTORY OBJECT_DIRECTORY, *POBJECT_DIRECTORY;
typedef struct _OBJECT_SYMLINK OBJECT_SYMLINK, *POBJECT_SYMLINK;

struct _OBJECT_TYPE_INFO
{
	// Properties
	int InvalidAttributes;
	
	int ValidAccessMask;
	
	bool NonPagedPool;
	
	// Virtual function table
	OBJ_OPEN_FUNC   Open;
	OBJ_CLOSE_FUNC  Close;
	OBJ_DELETE_FUNC Delete;
	OBJ_PARSE_FUNC  Parse;
	OBJ_SECURE_FUNC Secure;
#ifdef DEBUG
	OBJ_DEBUG_FUNC  Debug;
#endif
};

struct _OBJECT_TYPE
{
	OBJECT_TYPE_INFO TypeInfo;
	
	// Mirror of OBJECT_GET_HEADER(.)->ObjectName
	const char* TypeName;
	
	// List of object types that use this type.
	// Each entry is a field inside OBJECT_HEADER.
	LIST_ENTRY ObjectListHead;
	
	// Number of objects that use this type.
	int TotalObjectCount;
};

struct _NONPAGED_OBJECT_HEADER
{
	POBJECT_TYPE ObjectType;
	
	// Put these in a union to save space.
	union
	{
		struct
		{
			int PointerCount;
			int HandleCount;
		};
		
		// Entry into the object type's list of objects. Valid if this is an object type.
		LIST_ENTRY TypeListEntry;
	};
	
	POBJECT_HEADER NormalHeader;
};

struct _OBJECT_HEADER
{
	char* ObjectName;
	
	// Entry into the parent directory's list of entries.
	// TODO: Replace with an AATREE entry
	LIST_ENTRY DirectoryListEntry;
	
	// Pointer to the parent directory.
	void* ParentDirectory;
	
	// Context for the parse function.
	// This is an opaque pointer, that is, not used by the object system
	// itself, but passed into calls to the type's Parse method.
	void* ParseContext;
	
	// Object behavior flags.
	int Flags;
	
	PNONPAGED_OBJECT_HEADER NonPagedObjectHeader;
	
	// The size of the body.
	size_t BodySize;
	
	char Body[0];
};

struct _OBJECT_SYMLINK
{
	// The path of the object that this symbolic link links to.
	// If NULL, the symbolic link is considered invalid and will
	// throw errors when a lookup is attempted on it.
	char* DestPath;
};

// Object is owned by kernel mode.
#define OB_FLAG_KERNEL (1 << 0)

// Object is permanent, i.e. cannot be deleted.
#define OB_FLAG_PERMANENT (1 << 1)

// Object is temporary, i.e. will be deleted when all references are removed.
#define OB_FLAG_TEMPORARY (1 << 2)

// This represents the body of an object directory.
// When a child is added to a directory, the directory's pointer count is incremented by one.
// This is to allow the directory to 'hang around' while its children are still around. They'll
// just be linked to an orphan directory.
struct _OBJECT_DIRECTORY
{
	// Head of the list of children.
	// TODO: Replace with an AATREE
	LIST_ENTRY ListHead;
	
	// Number of children.
	int Count;
};

#define OBJECT_GET_HEADER(PBody) CONTAINING_RECORD(PBody, OBJECT_HEADER, Body)

#define OB_PATH_SEPARATOR ('\\')

#define OB_MAX_PATH_LENGTH (256)

// Initialization
bool ObInitSystem();

// Kernel mode API
BSTATUS ObCreateObjectType(
	const char* TypeName,
	POBJECT_TYPE_INFO TypeInfo,
	POBJECT_TYPE* OutObjectType
);

BSTATUS ObCreateObject(
	void** OutObject,
	POBJECT_DIRECTORY ParentDirectory,
	POBJECT_TYPE ObjectType,
	const char* ObjectName,
	int Flags,
	bool NonPaged,
	void* ParseContext,
	size_t BodySize
);

BSTATUS ObCreateDirectoryObject(
	POBJECT_DIRECTORY* OutDirectory,
	POBJECT_DIRECTORY ParentDirectory,
	const char* Name,
	int Flags
);

BSTATUS ObCreateSymbolicLinkObject(
	void** OutObject,
	const char* LinkTarget,
	const char* ObjectName,
	int Flags
);
