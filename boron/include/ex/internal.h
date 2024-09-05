/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ex/internal.h
	
Abstract:
	This header file defines executive internal calls.
	
	This must only be included by the kernel and will not expose
	anything if included from a driver.
	
Author:
	iProgramInCpp - 3 September 2024
***/
#pragma once

#ifdef KERNEL

// Safely duplicate a string from user space. Use MmFreePool to free the space allocated by the string.
BSTATUS ExiDuplicateUserString(char** OutNewString, const char* UserString, size_t StringLength);

// Safely copy an OBJECT_ATTRIBUTES instance.
BSTATUS ExiCopySafeObjectAttributes(POBJECT_ATTRIBUTES OutNewAttributes, POBJECT_ATTRIBUTES UserAttributes);

// Dispose of a copied OBJECT_ATTRIBUTES instance.
void ExiDisposeCopiedObjectAttributes(POBJECT_ATTRIBUTES Attributes);

typedef BSTATUS(*EX_OBJECT_CREATE_METHOD)(void* Object, void* Context);

// Create an object based on a user system service call.  This handles safety in all aspects, including
// copying the object attributes to kernel pool, dereferencing the root directory, copying the handle into
// the OutHandle pointer, etc.
BSTATUS ExiCreateObjectUserCall(
	PHANDLE OutHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	POBJECT_TYPE ObjectType,
	size_t ObjectBodySize,
	EX_OBJECT_CREATE_METHOD CreateMethod,
	void* CreateContext
);

// Opens an object from a user system service call.  This ensures that code is not duplicated.
BSTATUS ExiOpenObjectUserCall(
	PHANDLE OutHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	POBJECT_TYPE ObjectType,
	int OpenFlags
);

#else
#error Only the kernel may include this
#endif
