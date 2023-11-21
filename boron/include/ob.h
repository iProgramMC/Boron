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

typedef struct OBJECT_ATTRIBUTES_tag
{
	HANDLE RootDirectory;
	
	// If RootDirectory is NULL, then must start with a "\".
	// Otherwise, must NOT start with a "\", since the name is relative to that directory.
	const char* ObjectName;
	
	int Attributes;
}
OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

typedef struct OBJECT_HEADER_tag
{
	int Type  : 8;
	int Flags : 24;
}
OBJECT_HEADER, *POBJECT_HEADER;

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

//BSTATUS ObCreateObjectType(const char* TypeName, POBJECT_TYPE_INITIALIZER ObjectTypeInitializer, 
