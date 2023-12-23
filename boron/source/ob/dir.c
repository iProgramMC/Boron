/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/create.c
	
Abstract:
	This module implements create routines for the
	object manager.
	
Author:
	iProgramInCpp - 23 December 2023
***/
#include "obp.h"

KMUTEX ObpRootDirectoryMutex;

// TODO

OBJECT_TYPE_INFO ObpDirectoryTypeInfo =
{
	// InvalidAttributes
	0,
	// ValidAccessMask
	0,
	// NonPagedPool
	true,
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
	NULL,
#endif
};
