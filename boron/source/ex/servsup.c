/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ex/servsup.c
	
Abstract:
	This module defines helper Executive functions.
	
Author:
	iProgramInCpp - 9 August 2024
***/
#include "exp.h"

BSTATUS ExiDuplicateUserString(char** OutNewString, const char* UserString, size_t StringLength)
{
	BSTATUS Status;
	
	Status = MmProbeAddress((void*) UserString, StringLength, false);
	if (FAILED(Status))
		return Status;
	
	char* Str = MmAllocatePool(POOL_PAGED, StringLength + 1);
	if (!Str)
		return STATUS_INSUFFICIENT_MEMORY;
	
	Str[StringLength] = 0;
	
	Status = MmSafeCopy(Str, UserString, StringLength);
	if (FAILED(Status))
	{
		MmFreePool(Str);
		return Status;
	}
	
	*OutNewString = Str;
	return STATUS_SUCCESS;
}

BSTATUS ExiCopySafeObjectAttributes(POBJECT_ATTRIBUTES OutNewAttrs, POBJECT_ATTRIBUTES UserAttrs)
{
	BSTATUS Status;
	
	if (FAILED(Status = MmSafeCopy(&OutNewAttrs->RootDirectory, &UserAttrs->RootDirectory, sizeof(HANDLE))))
		return false;
	
	if (FAILED(Status = MmSafeCopy(&OutNewAttrs->ObjectNameLength, &UserAttrs->ObjectNameLength, sizeof(size_t))))
		return false;
	
	char* Name = NULL;
	if (FAILED(Status = ExiDuplicateUserString(&Name, UserAttrs->ObjectName, OutNewAttrs->ObjectNameLength)))
		return false;
	
	OutNewAttrs->ObjectName = Name;
	return STATUS_SUCCESS;
}

void ExiDisposeCopiedObjectAttributes(POBJECT_ATTRIBUTES Attributes)
{
	if (Attributes->ObjectName)
		MmFreePool((char*)Attributes->ObjectName);
	
	Attributes->ObjectName = NULL;
	Attributes->ObjectNameLength = 0;
	Attributes->RootDirectory = HANDLE_NONE;
}

// Creates an object from a user system service call.  This ensures that code is not duplicated.
BSTATUS ExiCreateObjectUserCall(
	PHANDLE OutHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	POBJECT_TYPE ObjectType,
	size_t ObjectBodySize,
	EX_OBJECT_CREATE_METHOD CreateMethod,
	void* CreateContext
)
{
	BSTATUS Status;
	OBJECT_ATTRIBUTES Attributes;
	bool CopyAttrs = KeGetPreviousMode() == MODE_USER;
	
	if (!ObjectAttributes)
	{
		Attributes.ObjectName = NULL;
		Attributes.ObjectNameLength = 0;
		Attributes.RootDirectory = HANDLE_NONE;
	}
	else if (CopyAttrs)
	{
		// If the previous mode was user, don't copy directly.  Instead, use
		// a helper function to copy everything into kernel mode.
		Status = ExiCopySafeObjectAttributes(&Attributes, ObjectAttributes);
		if (FAILED(Status))
			return Status;
	}
	else
	{
		Attributes = *ObjectAttributes;
	}
	
	void* ParentDirectory = NULL;
	
	// Reference the parent directory.
	if (Attributes.RootDirectory != HANDLE_NONE)
	{
		Status = ObReferenceObjectByHandle(Attributes.RootDirectory, ObDirectoryType, &ParentDirectory);
		if (FAILED(Status))
			goto Fail;
	}
	
	void* OutObject = NULL;
	Status = ObCreateObject(
		&OutObject,
		ParentDirectory,
		ObjectType,
		Attributes.ObjectName,
		(KeGetPreviousMode() == MODE_KERNEL) ? OB_FLAG_KERNEL : 0,
		NULL,
		ObjectBodySize
	);
	
	if (FAILED(Status))
		goto Fail;
	
	// The object was created, so initialize it.
	Status = CreateMethod(OutObject, CreateContext);
	if (FAILED(Status))
		goto Fail;
	
	// OK, now insert this object into the handle table.  After insertion, dereferencing
	// it (removing the initial reference) is mandatory in either case.
	HANDLE Handle = HANDLE_NONE;
	Status = ObInsertObject(OutObject, &Handle);
	
	ObDereferenceObject(OutObject);
	
	if (FAILED(Status))
		goto FailUndo;
	
	// Finally, copy the handle.
	Status = MmSafeCopy(OutHandle, &Handle, sizeof(HANDLE));
	if (SUCCEEDED(Status))
	{
		// Yay! The creation went smoothly. Return success status.
		return Status;
	}
	
	// Nope. We will need to dispose of this object and return an error status.
	// First, close the handle.  The status code is ignored because closing it is sure
	// to succeed, and we want to return the cause of the error, and not a handle table
	// error (if it exists).
	(void) ObClose(Handle);

FailUndo:
	// The initial reference has been undone, we will also need to unlink the object if
	// needed.  Note that the status is ignored.  This is because unlinking the object
	// can only fail if the object was not linked to a directory, in which case it has
	// no reference bias from the directory.
	(void) ObUnlinkObject(OutObject);

Fail:
	// We will also need to dereference the parent directory if we referenced one.
	if (ParentDirectory)
		ObDereferenceObject(ParentDirectory);
	
	if (CopyAttrs)
		ExiDisposeCopiedObjectAttributes(&Attributes);
	
	return Status;
}

// Opens an object from a user system service call.  This ensures that code is not duplicated.
BSTATUS ExiOpenObjectUserCall(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes, POBJECT_TYPE ObjectType, int OpenFlags)
{
	BSTATUS Status;
	OBJECT_ATTRIBUTES Attributes;
	bool CopyAttrs = KeGetPreviousMode() == MODE_USER;
	
	if (!ObjectAttributes)
	{
		Attributes.ObjectName = NULL;
		Attributes.ObjectNameLength = 0;
		Attributes.RootDirectory = HANDLE_NONE;
	}
	else if (CopyAttrs)
	{
		// If the previous mode was user, don't copy directly.  Instead, use
		// a helper function to copy everything into kernel mode.
		Status = ExiCopySafeObjectAttributes(&Attributes, ObjectAttributes);
		if (FAILED(Status))
			return Status;
	}
	else
	{
		Attributes = *ObjectAttributes;
	}
	
	HANDLE Handle = HANDLE_NONE;
	Status = ObOpenObjectByName(
		Attributes.ObjectName,
		Attributes.RootDirectory,
		OpenFlags,
		ObjectType,
		&Handle
	);
	
	if (FAILED(Status))
		goto Fail;
	
	// Finally, copy the handle.
	Status = MmSafeCopy(OutHandle, &Handle, sizeof(HANDLE));
	if (SUCCEEDED(Status))
	{
		// Yay! The creation went smoothly. Return success status.
		return Status;
	}
	
	// Nope. We will need to close this object and return an error status.
	// First, close the handle.  The status code is ignored because closing it is sure
	// to succeed, and we want to return the cause of the error, and not a handle table
	// error (if it exists).
	(void) ObClose(Handle);
	
Fail:
	if (CopyAttrs)
		ExiDisposeCopiedObjectAttributes(&Attributes);
	
	return Status;
}