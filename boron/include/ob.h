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
#include <ex.h>

typedef enum _OB_OPEN_REASON
{
	OB_CREATE_HANDLE,
	OB_OPEN_HANDLE,
	OB_DUPLICATE_HANDLE,
	OB_INHERIT_HANDLE,
}
OB_OPEN_REASON;

// Lets the object know a handle to it was opened.
typedef void(*OBJ_OPEN_FUNC)  (void* Object, int HandleCount, OB_OPEN_REASON OpenReason);

// Lets the object know a handle to it was closed.
typedef void(*OBJ_CLOSE_FUNC) (void* Object, int HandleCount);

// Lets the object know that it should tear down because the system will delete it.
typedef void(*OBJ_DELETE_FUNC)(void* Object);

//
// Parse a path using this object.
//
// N.B.:
//  1. In the implementation of the Parse method, if the Parse method performs a lookup
//     using ObReferenceObjectByName, then it MUST pass LoopCount into the LoopCount parameter!
//     (I mean, you can always pass LoopCount + 1 if you're stubborn like that, but you will
//     run out of loops twice as quickly if some guy decides they want to create a symlink loop)
//
//  2. The returned Object will have 1 extra reference, if there is one.
//
//  3. The "Name" value will be overwritten if an object was found, with the following:
//     - NULL: This is the desired object, or
//     - A valid offset of the original value of 'Name' which can be used to lookup 
//
//  4. The passed in ParseObject will have a net zero difference in reference count after the
//     call (so, the initial reference won't be dereferenced while inside of the function).
//     The dereference job falls onto the caller (in most cases, ObReferenceObjectByName)
//
typedef BSTATUS(*OBJ_PARSE_FUNC) (
	void* ParseObject,
	const char** Name,
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
	
	bool MaintainHandleCount;
	
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
	
	int PointerCount;
	int HandleCount;
	
	POBJECT_HEADER NormalHeader;
};

struct _OBJECT_HEADER
{
	char* ObjectName;
	
	// Entry into the parent directory's list of entries.
	// TODO: Replace with an AATREE entry
	//
	// If object was deleted at high IPL, this is the entry into the list of reaped objects
	union
	{
		LIST_ENTRY DirectoryListEntry;
		LIST_ENTRY ReapedListEntry;
	};
	
	// Pointer to the parent directory.
	POBJECT_DIRECTORY ParentDirectory;
	
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

// Object creation flags:
enum
{
	// Object is owned by kernel mode.
	OB_FLAG_KERNEL    = (1 << 0),
	// Object is permanent, i.e. cannot be deleted.
	OB_FLAG_PERMANENT = (1 << 1),
	// Object is allocated in non-paged memory.
	OB_FLAG_NONPAGED  = (1 << 2),
	// The object, when created, isn't associated with a directory.
	OB_FLAG_NO_DIRECTORY = (1 << 3),
};

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

// Object open flags:
enum
{
	// Handle may be inherited by child processes.
	OB_OPEN_INHERIT   = (1 << 0),
	// No other process can open this handle while the current process maintains a handle.
	OB_OPEN_EXCLUSIVE = (1 << 1),
	// If the final path component is a symbolic link, open the symbolic link object itself
	// instead of its referenced object (the latter is the default behavior)
	OB_OPEN_SYMLINK   = (1 << 2),
};

// ========== Initialization ==========
bool ObInitSystem();

// ========== Kernel mode API ==========

// Creates an object type.
BSTATUS ObCreateObjectType(
	const char* TypeName,
	POBJECT_TYPE_INFO TypeInfo,
	POBJECT_TYPE* OutObjectType
);

// Creates an object of a certain type.
BSTATUS ObCreateObject(
	void** OutObject,
	POBJECT_DIRECTORY ParentDirectory,
	POBJECT_TYPE ObjectType,
	const char* ObjectName,
	int Flags,
	void* ParseContext,
	size_t BodySize
);

// Creates a directory object.
BSTATUS ObCreateDirectoryObject(
	POBJECT_DIRECTORY* OutDirectory,
	POBJECT_DIRECTORY ParentDirectory,
	const char* Name,
	int Flags
);

// Creates a symbolic link object.
BSTATUS ObCreateSymbolicLinkObject(
	void** OutObject,
	const char* LinkTarget,
	const char* ObjectName,
	int Flags
);

// Adds 1 to the internal reference count of the object.
void ObReferenceObjectByPointer(void* Object);

// Takes a reference away from the internal ref count of the object.
// If the reference count hits zero, the object will be deleted.
void ObDereferenceObject(void* Object);

// Links an object to a directory.
// The name field is:
// - mandatory if the object is anonymous (does not have a name), or
// - ignored if the object is not anonymous (has a name)
BSTATUS ObLinkObject(POBJECT_DIRECTORY Directory, void* Object, const char* Name);

// Unlinks an object from its parent directory.
BSTATUS ObUnlinkObject(void* Object);

// Inserts an object into the current process' handle table.
BSTATUS ObInsertObject(void* Object, PHANDLE OutHandle);

// Opens an object by name.
//
// N.B. Specifying HANDLE_NONE to RootDirectory means that the lookup is absolute.
BSTATUS ObOpenObjectByName(
	const char* Path,
	HANDLE RootDirectory,
	int OpenFlags,
	POBJECT_TYPE ExpectedType,
	PHANDLE OutHandle
);

// References an object by handle and returns its pointer.
// The caller must call ObDereferenceObject when done with the returned object.
BSTATUS ObReferenceObjectByHandle(HANDLE Handle, void** OutObject);

// References an object by name and returns its pointer.
// The caller must call ObDereferenceObject when done with the returned object.
//
// N.B. Specifying NULL for InitialParseObject means that the lookup is absolute.
//
// N.B. The InitialParseObject will have a net zero difference in reference count
//      across calls to this function.
BSTATUS ObReferenceObjectByName(
	const char* Path,
	void* InitialParseObject,
	int OpenFlags,
	POBJECT_TYPE ExpectedType,
	void** OutObject
);

// Closes a handle.
BSTATUS ObClose(HANDLE Handle);

// Gets the object's type.
POBJECT_TYPE ObGetObjectType(void* Object);