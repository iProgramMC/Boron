/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	obs.h
	
Abstract:
	This header file contains the publicly exposed structure
	definitions for Boron's Object Subsystem.
	
Author:
	iProgramInCpp - 22 April 2025
***/
#pragma once

#include "handle.h"

#define OB_PATH_SEPARATOR ('/')
#define OB_MAX_PATH_LENGTH (256)

// Object open flags:
enum
{
	// Handle may not be inherited by child processes.
	OB_OPEN_NO_INHERIT = (1 << 0),
	
	// No other process may open this handle while the current process maintains a handle.
	// Unimplemented: OB_OPEN_EXCLUSIVE = (1 << 1),
	
	// If the final path component is a symbolic link, open the symbolic link object itself
	// instead of its referenced object (the latter is the default behavior)
	OB_OPEN_SYMLINK = (1 << 2),
	
	// If the root directory's handle wasn't specified, then this opens a file in the
	// absolute root of the object namespace, instead of in the file system root.
	//
	// This flag is ignored for non-file objects.
	OB_OPEN_OBJECT_NAMESPACE = (1 << 3),
	
	// This flag is set when user mode requests an object.  This flag is ignored if passed
	// in from user mode.
	OB_OPEN_USER_MODE = (1 << 4),
};

typedef struct _OBJECT_ATTRIBUTES
{
	HANDLE RootDirectory;
	const char* ObjectName;
	size_t ObjectNameLength;
	int OpenFlags;
}
OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;
