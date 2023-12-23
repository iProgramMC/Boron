/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/link.c
	
Abstract:
	This module implements the symbolic link object type for
	the object manager.
	
Author:
	iProgramInCpp - 23 December 2023
***/
#include "obp.h"

// TODO

OBJECT_TYPE_INFO ObpSymbolicLinkTypeInfo =
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

