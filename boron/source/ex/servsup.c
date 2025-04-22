/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ex/servsup.c
	
Abstract:
	This module defines routines to support the implementation
	of system services.
	
Author:
	iProgramInCpp - 9 August 2024
***/
#include "exp.h"
#include <ps.h>

BSTATUS ExiDuplicateUserString(char** OutNewString, const char* UserString, size_t StringLength)
{
	BSTATUS Status;
	
	Status = MmProbeAddress((void*) UserString, StringLength, false, KeGetPreviousMode());
	if (FAILED(Status))
		return Status;
	
	char* Str = MmAllocatePool(POOL_PAGED, StringLength + 1);
	if (!Str)
		return STATUS_INSUFFICIENT_MEMORY;
	
	Str[StringLength] = 0;
	
	Status = MmSafeCopy(Str, UserString, StringLength, KeGetPreviousMode(), false);
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
	
	// Copy the entire struct.  Note that any pointers will be invalid to access (since they're potentially
	// provided from userspace), so we will replace them below.
	
	if (FAILED(Status = MmSafeCopy(OutNewAttrs, UserAttrs, sizeof(OBJECT_ATTRIBUTES), KeGetPreviousMode(), false)))
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
	Attributes->OpenFlags = 0;
}

// Does the same operation as ObReferenceObjectByHandle, except translates
// special handle values to references to the actual object.
//
// (e.g. CURRENT_PROCESS_HANDLE is translated to the current process)
//
// NOTE: ExReferenceObjectByHandle(CURRENT_THREAD_HANDLE) is not defined for threads that weren't created via Ps.
//
// NOTE: ExReferenceObjectByHandle(CURRENT_PROCESS_HANDLE) is defined for the System process.
BSTATUS ExReferenceObjectByHandle(HANDLE Handle, POBJECT_TYPE ExpectedType, void** OutObject)
{
	if (ExpectedType == PsThreadObjectType && Handle == CURRENT_THREAD_HANDLE)
	{
		void* Thrd = KeGetCurrentThread();
		*OutObject = Thrd;
		ObReferenceObjectByPointer(Thrd);
		return STATUS_SUCCESS;
	}
	
	if (ExpectedType == PsProcessObjectType && Handle == CURRENT_PROCESS_HANDLE)
	{
		void* Proc = PsGetCurrentProcess();
		*OutObject = Proc;
		ObReferenceObjectByPointer(Proc);
		return STATUS_SUCCESS;
	}
	
	return ObReferenceObjectByHandle(Handle, ExpectedType, OutObject);
}

// Creates an object from a user system service call.  This ensures that code is not duplicated.
BSTATUS ExCreateObjectUserCall(
	PHANDLE OutHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	POBJECT_TYPE ObjectType,
	size_t ObjectBodySize,
	EX_OBJECT_CREATE_METHOD CreateMethod,
	void* CreateContext,
	int PoolFlag
)
{
	BSTATUS Status;
	OBJECT_ATTRIBUTES Attributes;
	bool CopyAttrs = KeGetPreviousMode() == MODE_USER;
	int Flags = 0;
	
	if (!CopyAttrs)
		Flags |= OB_FLAG_KERNEL;
	
	if (!ObjectAttributes)
	{
		Attributes.ObjectName = NULL;
		Attributes.ObjectNameLength = 0;
		Attributes.RootDirectory = HANDLE_NONE;
		Attributes.OpenFlags = 0;
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
	
	if (!Attributes.ObjectNameLength)
		Flags |= OB_FLAG_NO_DIRECTORY;
	
	if (PoolFlag & POOL_NONPAGED)
		Flags |= OB_FLAG_NONPAGED;
	
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
		Flags,
		NULL,
		ObjectBodySize
	);
	
	if (FAILED(Status))
		goto Fail;
	
	// The object was created, so initialize it.
	Status = CreateMethod(OutObject, CreateContext);
	if (FAILED(Status))
		goto Fail;
	
	// OK, now insert this object into the handle table.
	HANDLE Handle = HANDLE_NONE;
	Status = ObInsertObject(OutObject, &Handle, Attributes.OpenFlags);
	
	if (FAILED(Status))
		goto FailUndo;
	
	// Finally, copy the handle.
	Status = MmSafeCopy(OutHandle, &Handle, sizeof(HANDLE), KeGetPreviousMode(), true);
	if (SUCCEEDED(Status))
	{
		// Yay! The creation went smoothly. Remove the initial reference (we still have
		// one reference from the handle table!) and return a success status.
		ObDereferenceObject(OutObject);
		return Status;
	}
	
	// Nope. We will need to dispose of this object and return an error status.
	// First, close the handle.  The status code is ignored because closing it is sure
	// to succeed, and we want to return the cause of the error, and not a handle table
	// error (if it exists).
	(void) ObClose(Handle);

FailUndo:
	// Unlink the object, if possible.  Note that the status is ignored.  This is because
	// unlinking the object can only fail if it isn't linked to a directory, in which case
	// there's no reference count bias to undo.
	(void) ObUnlinkObject(OutObject);
	
	// Finally, remove the initial (ObCreateObject given) reference.
	ObDereferenceObject(OutObject);

Fail:
	// We will also need to dereference the parent directory if we referenced one.
	if (ParentDirectory)
		ObDereferenceObject(ParentDirectory);
	
	if (CopyAttrs)
		ExiDisposeCopiedObjectAttributes(&Attributes);
	
	return Status;
}

// Opens an object from a user system service call.  This ensures that code is not duplicated.
BSTATUS ExOpenObjectUserCall(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes, POBJECT_TYPE ObjectType)
{
	BSTATUS Status;
	OBJECT_ATTRIBUTES Attributes;
	bool CopyAttrs = KeGetPreviousMode() == MODE_USER;
	
	if (!ObjectAttributes)
		return STATUS_INVALID_PARAMETER;
	
	if (CopyAttrs)
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
		Attributes.OpenFlags,
		ObjectType,
		&Handle
	);
	
	if (FAILED(Status))
		goto Fail;
	
	// Finally, copy the handle.
	Status = MmSafeCopy(OutHandle, &Handle, sizeof(HANDLE), KeGetPreviousMode(), true);
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

// Closes a handle to an object.
BSTATUS OSClose(HANDLE Handle)
{
	return ObClose(Handle);
}

// Dummy.
BSTATUS OSDummy()
{
	LogMsg("Dummy system call invoked from %p.", KeGetCurrentThread());
	return 0;
}
