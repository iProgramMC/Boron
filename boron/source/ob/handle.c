/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ob/handle.c
	
Abstract:
	This module implements functions to manipulate the object
	handle table for the object manager.
	
Author:
	iProgramInCpp - 11 June 2024
***/
#include "obp.h"
#include <ex.h>

extern POBJECT_DIRECTORY ObpRootDirectory;

BSTATUS ObReferenceObjectByHandle(HANDLE Handle, void** OutObject)
{
	BSTATUS Status;
	PEPROCESS Process = PsGetCurrentProcess();
	
	// Look for the handle in the table.
	void* Object = NULL;
	Status = ExGetPointerFromHandle(Process->HandleTable, Handle, &Object);
	if (FAILED(Status))
		return Status;
	
	// If it returned STATUS_SUCCESS, it should have returned a valid object.
	ASSERT(Object);
	
	// Reference the object.
	ObReferenceObjectByPointer(Object);
	
	*OutObject = Object;
	
	return STATUS_SUCCESS;
}

bool ObpCloseHandle(void* Object, UNUSED void* Context)
{
	POBJECT_HEADER Header = OBJECT_GET_HEADER(Object);
	POBJECT_TYPE Type = Header->NonPagedObjectHeader->ObjectType;
	
	OBJ_CLOSE_FUNC CloseMethod = Type->TypeInfo.Close;
	
	int HandleCount = 0;
	if (Type->TypeInfo.MaintainHandleCount)
		HandleCount = AtFetchAdd(Header->NonPagedObjectHeader->HandleCount, -1);
	
	if (CloseMethod)
		CloseMethod(Object, HandleCount);
	
	ObDereferenceObject(Object);
	return true;
}

BSTATUS ObClose(HANDLE Handle)
{
	PEPROCESS Process = PsGetCurrentProcess();
	return ExDeleteHandle(Process->HandleTable, Handle, ObpCloseHandle, NULL);
}

BSTATUS ObpInsertObject(void* Object, PHANDLE OutHandle, OB_OPEN_REASON OpenReason)
{
	BSTATUS Status;
	PEPROCESS Process = PsGetCurrentProcess();
	
	// Add a reference to this object, because it is now also referenced in the handle table.
	ObReferenceObjectByPointer(Object);
	
	Status = ExCreateHandle(Process->HandleTable, Object, OutHandle);
	if (FAILED(Status))
	{
		// Couldn't add it. Then dereference the object.
		ObDereferenceObject(Object);
		return Status;
	}
	
	POBJECT_HEADER Header = OBJECT_GET_HEADER(Object);
	PNONPAGED_OBJECT_HEADER NPHeader = Header->NonPagedObjectHeader;
	
	bool MaintainHandleCount = NPHeader->ObjectType->TypeInfo.MaintainHandleCount;
	int HandleCount = 0;
	
	// Increment the handle count if needed.
	if (MaintainHandleCount)
		HandleCount = AtAddFetch(NPHeader->HandleCount, 1);
	
	// Call the object type's Open method.
	OBJ_OPEN_FUNC OpenMethod = NPHeader->ObjectType->TypeInfo.Open;
	if (OpenMethod)
		OpenMethod(Object, HandleCount, OpenReason);
	
	// Added successfully, so return.
	return Status;
}

BSTATUS ObInsertObject(void* Object, PHANDLE OutHandle)
{
	return ObpInsertObject(Object, OutHandle, OB_CREATE_HANDLE);
}

BSTATUS ObReferenceObjectByName(
	const char* ObjectName,
	void* InitialParseObject,
	int OpenFlags,
	POBJECT_TYPE ExpectedType,
	void** OutObject)
{
	return ObpLookUpObjectPath(
		InitialParseObject,
		ObjectName,
		ExpectedType,
		0,             // LoopCount
		OpenFlags,
		OutObject      // FoundObject
	);
}

BSTATUS ObOpenObjectByName(
	const char* ObjectName,
	HANDLE RootDirectory,
	int OpenFlags,
	POBJECT_TYPE ExpectedType,
	PHANDLE OutHandle)
{
	BSTATUS Status;
	PEPROCESS Process = PsGetCurrentProcess();
	
	void* InitialParseObject = NULL;
	
	if (RootDirectory != HANDLE_NONE)
	{
		Status = ExGetPointerFromHandle(Process->HandleTable, RootDirectory, &InitialParseObject);
		if (FAILED(Status))
			return Status;
	}
	
	void* Object = NULL;
	
	Status = ObReferenceObjectByName(
		ObjectName,
		InitialParseObject,
		OpenFlags,
		ExpectedType,
		&Object
	);
	
	if (FAILED(Status))
		return Status;
	
	// OK, the object was found.
	Status = ObpInsertObject(Object, OutHandle, OB_OPEN_HANDLE);

	// De-reference the object because ObpLookUpObjectPath adds a reference to the object.
	ObDereferenceObject(Object);
	return Status;
}
