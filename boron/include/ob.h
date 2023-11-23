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

#include <main.h>

typedef uintptr_t HANDLE;

typedef struct OBJECT_HEADER_tag OBJECT_HEADER, *POBJECT_HEADER;

typedef struct OBJECT_TYPE_tag OBJECT_TYPE, *POBJECT_TYPE;

struct OBJECT_HEADER_tag
{
	// Pointer to name in non-paged memory. Must be available for the lifetime of the object.
	const char* Name;
	
	// Type of object.  Specifies actions that can be performed on the object.
	// Is in non-paged memory.
	POBJECT_TYPE Type;
	
	// Reference count.
	int References;
	
	// Attributes.
	int Attributes;
	
	// Pointer to parent. Is in non-paged memory.
	POBJECT_HEADER Parent;
	
	// Entry within the list of children objects of our parent.
	LIST_ENTRY EntryList;
	
	// List of children objects. Must be empty if this is not a directory.
	LIST_ENTRY ChildList;
};

// TODO

// ***** Function pointer definitions. *****

// Called when a handle to the object is created.
//typedef void(*OBJ_OPEN_METHOD)();
//typedef OBJ_CLOSE_METHOD;
//typedef OBJ_PARSE_METHOD;
////?typedef OBJ_SECURE_METHOD;
//typedef OBJ_DELETE_METHOD;


#ifdef DEBUG
// Dump information about the object to debugger.
typedef void(*OBJ_DUMP_METHOD)(POBJECT_HEADER Object);
#endif

typedef struct OBJECT_TYPE_INITIALIZER_tag
{
	// Attributes that are forbidden for use in the object header with this type.
	// An attempt to create such an object will result in an invalid parameter error.
	int ForbiddenAttributes;
	
	// ***** Function Pointers *****
	//OBJ_OPEN_METHOD Open;
	//OBJ_CLOSE_METHOD Close;
	//OBJ_PARSE_METHOD Parse;
	////OBJ_SECURE_METHOD Secure;
	//OBJ_DELETE_METHOD Delete;
	
#ifdef DEBUG
	OBJ_DUMP_METHOD Dump;
#endif
}
OBJECT_TYPE_INITIALIZER, *POBJECT_TYPE_INITIALIZER;

struct OBJECT_TYPE_tag
{
	OBJECT_HEADER Header;
	
	// Object type initializer. Contains forbidden attributes, function pointers, etc.
	OBJECT_TYPE_INITIALIZER Initializer;
}

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


