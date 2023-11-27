/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob.h
	
Abstract:
	This header file contains the definitions related to
	the Boron object manager.
	
Author:
	iProgramInCpp - 19 November 2023
***/
#pragma once

#include <ps.h>

typedef uintptr_t HANDLE;
typedef int OATTRIBUTES;

typedef int ACCESS_MASK,  *PACCESS_MASK;
typedef int ACCESS_STATE, *PACCESS_STATE;

typedef struct OBJECT_TYPE_tag OBJECT_TYPE, *POBJECT_TYPE;

typedef enum OBJ_OPEN_REASON_tag
{
	OBJ_OPEN_CREATE_HANDLE,
	OBJ_OPEN_OPEN_HANDLE,
	OBJ_OPEN_DUPLICATE_HANDLE,
	OBJ_OPEN_INHERIT_HANDLE,
}
OBJ_OPEN_REASON;

typedef struct OBJECT_ATTRIBUTES_tag
{
	HANDLE RootDirectory;
	const char* ObjectName;
	OATTRIBUTES Attributes;
}
OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

// TODO

// ***** Function pointer definitions. *****

// Called when a handle to the object is created.
typedef void(*OBJ_OPEN_METHOD)(int OpenReason, PEPROCESS Process, void* Object, ACCESS_MASK GrantedAccess, int HandleCount);

// Called when a handle to the object is deleted.
typedef void(*OBJ_CLOSE_METHOD)(PEPROCESS Process, void* Object, ACCESS_MASK GrantedAccess, int HandleCount);

// Called when the reference count of an object is decremented to zero, and the object is temporary.
typedef void(*OBJ_DELETE_METHOD)(void* Object);

typedef void(*OBJ_PARSE_METHOD)(
	void* ParseObject,
	POBJECT_TYPE ObjectType,
	PACCESS_STATE AccessState,
	KPROCESSOR_MODE AccessMode,
	OATTRIBUTES Attributes,
	const char** CompleteName,
	const char** RemainingName,
	void* Context,
	void** Object
);

// TODO: OBJ_SECURE_METHOD

#ifdef DEBUG
// Dump information about the object to debugger.
typedef void(*OBJ_DUMP_METHOD)(void* Object);
#endif

typedef struct OBJECT_TYPE_INITIALIZER_tag
{
	// Attributes that are forbidden for use in the object header with this type.
	// An attempt to create such an object will result in an invalid parameter error.
	int ForbiddenAttributes;
	
	// Whether to maintain handle count.
	bool MaintainHandleCount;
	
	// ***** Function Pointers *****
	OBJ_OPEN_METHOD Open;
	OBJ_CLOSE_METHOD Close;
	OBJ_PARSE_METHOD Parse;
	//OBJ_SECURE_METHOD Secure;
	OBJ_DELETE_METHOD Delete;
	
#ifdef DEBUG
	OBJ_DUMP_METHOD Dump;
#endif
}
OBJECT_TYPE_INITIALIZER, *POBJECT_TYPE_INITIALIZER;

struct OBJECT_TYPE_tag
{
	// Object type initializer. Contains forbidden attributes, function pointers, etc.
	OBJECT_TYPE_INITIALIZER Initializer;
	
	// Offset to dispatcher object.  If zero (the object header goes first), then
	// can't be used as an argument to BrnWaitFor*Object(s).
	size_t OffsetDispatchObject;
};

// The open handle is to be inherited by child processes
// whenever the calling process creates a new process.
#define OBJ_INHERIT (1 << 0)

// The object is to be accessed exclusively from within the creator processor.
// Mutually exclusive with OBJ_INHERIT.
#define OBJ_EXCLUSIVE (1 << 1)

// The object is to be created as a permanent object.
#define OBJ_PERMANENT (1 << 2)

// Name lookup within the object should ignore case distinction.
#define OBJ_CASE_INSENSITIVE (1 << 3)

// Return a handle to a pre-existing object if an object by the same name
// already exists. If creating, and the name doesn't exist, create the object.
#define OBJ_OPENIF (1 << 4)

BSTATUS ObCreateObjectType(
	const char* TypeName,
	POBJECT_TYPE_INITIALIZER Initializer,
	size_t OffsetDispatchObject,
	POBJECT_TYPE *ObjectType
);

BSTATUS ObCreateObject(
	KPROCESSOR_MODE ProbeMode,
	POBJECT_TYPE ObjectType,
	POBJECT_ATTRIBUTES ObjectAttributes,
	KPROCESSOR_MODE OwnershipMode,
	void* ParseContext,
	void** Object
);
