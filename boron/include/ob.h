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
#include <ex/handtab.h>

#include <obs.h>

typedef enum _OB_OPEN_REASON
{
	OB_CREATE_HANDLE,
	OB_OPEN_HANDLE,
	OB_DUPLICATE_HANDLE,
	OB_INHERIT_HANDLE,
}
OB_OPEN_REASON;

// Lets the object know a handle to it was opened.
//
// Can return an error status in which case the opening
// of the object can be interrupted.
typedef BSTATUS(*OBJ_OPEN_FUNC)(void* Object, int HandleCount, OB_OPEN_REASON OpenReason);

// Lets the object know a handle to it was closed.
typedef void(*OBJ_CLOSE_FUNC) (void* Object, int HandleCount);

// Lets the object know that it should tear down because the system will delete it.
typedef void(*OBJ_DELETE_FUNC)(void* Object);

// Lets the object specify a different object when a handle is being duplicated.
//
// If this function doesn't return NULL, it shall return a pointer to an object
// with a reference count of at least one.
typedef void*(*OBJ_DUPLICATE_FUNC)(void* Object, int OpenReason);

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
	OBJ_OPEN_FUNC      Open;
	OBJ_CLOSE_FUNC     Close;
	OBJ_DELETE_FUNC    Delete;
	OBJ_PARSE_FUNC     Parse;
	OBJ_SECURE_FUNC    Secure;
	OBJ_DUPLICATE_FUNC Duplicate;
#ifdef DEBUG
	OBJ_DEBUG_FUNC     Debug;
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
#ifdef DEBUG
#define OBJECT_HEADER_SIGNATURE 0x6953624F // 'ObSi'
	// Signature.  In debug mode, this is checked against for reference/dereference operations,
	// among other things, to ensure that non-object-manager-managed objects aren't accidentally
	// used.
	int Signature;

#ifdef IS_32_BIT
	// This dummy is present to make sure the object header's struct size is aligned to 8 bytes.
	int Dummy;
#endif
#endif
	
	// Object behavior flags.
	int Flags;
	
	// Pool-allocated pointer to characters - the object's name.
	// May be NULL, in which case the object is nameless and does not belong to
	// the global object namespace.
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
	
	PNONPAGED_OBJECT_HEADER NonPagedObjectHeader;
	
	// The size of the body.
	size_t BodySize;
	
	char Body[0];
};

// Because the object handle table manager requires that pointers stored
// inside MUST be aligned to 8 bytes, we should do this.  The header will
// most likely be aligned to 8 bytes due to pool allocation semantics, but
// we need to make it a given that the body is also aligned to 8 bytes.
static_assert((sizeof(OBJECT_HEADER) & 0x7) == 0);

struct _OBJECT_SYMLINK
{
	// The path of the object that this symbolic link links to.
	// If NULL, the symbolic link is considered invalid and will
	// throw errors when a lookup is attempted on it.
	//
	// This pointer is pool-allocated.
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
	// The mutex guarding this object directory.
	KMUTEX Mutex;
	
	// Head of the list of children.
	// TODO: Replace with an AATREE
	LIST_ENTRY ListHead;
	
	// Number of children.
	int Count;
};

#define OBJECT_GET_HEADER(PBody) CONTAINING_RECORD(PBody, OBJECT_HEADER, Body)

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

// Creates an object of a certain type, and optionally calls a callback
// function, before adding it to the destination directory.
typedef BSTATUS(*OB_INIT_CALLBACK)(void* Object, void* CallbackContext);

BSTATUS ObCreateObjectCallback(
	void** OutObject,
	POBJECT_DIRECTORY ParentDirectory,
	POBJECT_TYPE ObjectType,
	const char* ObjectName,
	int Flags,
	void* ParseContext,
	size_t BodySize,
	OB_INIT_CALLBACK Callback,
	void* CallbackContext
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
void* ObReferenceObjectByPointer(void* Object);

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
BSTATUS ObInsertObject(void* Object, PHANDLE OutHandle, int OpenFlags);

// Inserts an object into the handle table of a specified process.
BSTATUS ObInsertObjectProcess(PEPROCESS Process, void* Object, PHANDLE OutHandle, int OpenFlags);

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
BSTATUS ObReferenceObjectByHandle(HANDLE Handle, POBJECT_TYPE ExpectedType, void** OutObject);

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

// Duplicates a handle table managed by the Object Manager.
BSTATUS ObDuplicateHandleTable(void** NewHandleTable, void* OldHandleTable);

// Deletes a handle table and closes every handle.
BSTATUS ObKillHandleTable(void* HandleTable);

// Matches a file name with the first segment of the path name,
// up until a backslash ('\') character within the `Path`.
// Returns 0 if not matched, >0 (length of path consumed) if matched.
size_t ObMatchPathName(const char* FileName, const char* Path);

// Object manager provided object types:

enum
{
	__OB_OBJECT_TYPE_TYPE,
	__OB_DIRECTORY_TYPE,
	__OB_SYMLINK_TYPE,
	__OB_BUILTIN_TYPE_COUNT
};

#ifdef KERNEL

// Creates a symbolic link to the root of the active installation.
// Must be used only once during init.
bool ObLinkRootDirectory();

#endif

#ifdef KERNEL

extern POBJECT_TYPE ObObjectTypeType;
extern POBJECT_TYPE ObDirectoryType;
extern POBJECT_TYPE ObSymbolicLinkType;

#else

extern POBJECT_TYPE ObGetBuiltInType(int Id);

#define ObObjectTypeType   ObGetBuiltInType(__OB_OBJECT_TYPE_TYPE)
#define ObDirectoryType    ObGetBuiltInType(__OB_DIRECTORY_TYPE)
#define ObSymbolicLinkType ObGetBuiltInType(__OB_SYMLINK_TYPE)

#endif


