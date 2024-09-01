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

typedef union
{
	struct
	{
	#ifdef IS_64_BIT
		uintptr_t Inherit     : 1;
		uintptr_t Spare       : 2;
		uintptr_t AddressBits : 62;
	#else
		uintptr_t Inherit     : 1;
		uintptr_t Spare       : 2;
		uintptr_t AddressBits : 30;
	#endif
	} U;
	
	void* Pointer;
}
OB_HANDLE_ITEM;

BSTATUS ObReferenceObjectByHandle(HANDLE Handle, POBJECT_TYPE ExpectedType, void** OutObject)
{
	BSTATUS Status;
	PEPROCESS Process = PsGetCurrentProcess();
	
	// Look for the handle in the table.
	OB_HANDLE_ITEM HandleItem;
	HandleItem.Pointer = NULL;
	HandleItem.U.AddressBits = 0; // simply to ignore a warning
	
	Status = ExGetPointerFromHandle(Process->HandleTable, Handle, &HandleItem.Pointer);
	if (FAILED(Status))
		return Status;
	
	// If it returned STATUS_SUCCESS, it should have returned a valid object.
	ASSERT(HandleItem.U.AddressBits);
	
	void* Object = (void*) ((uintptr_t)HandleItem.U.AddressBits << 3);
	
	// Check if the object is of the correct type.
	if (ExpectedType && OBJECT_GET_HEADER(Object)->NonPagedObjectHeader->ObjectType != ExpectedType)
	{
		ExUnlockHandleTable(Process->HandleTable);
		return STATUS_TYPE_MISMATCH;
	}
	
	// Reference the object.
	ObReferenceObjectByPointer(Object);
	
	// Unlock the process' handle table.
	ExUnlockHandleTable(Process->HandleTable);
	
	*OutObject = Object;
	
	return STATUS_SUCCESS;
}

bool ObpCloseHandle(void* HandleItemV, UNUSED void* Context)
{
	OB_HANDLE_ITEM HandleItem;
	HandleItem.U.AddressBits = 0; // simply to ignore a warning
	HandleItem.Pointer = HandleItemV;
	void* Object = (void*) ((uintptr_t)HandleItem.U.AddressBits << 3);
	
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

BSTATUS ObpInsertObject(void* Object, PHANDLE OutHandle, OB_OPEN_REASON OpenReason, int OpenFlags)
{
	BSTATUS Status;
	PEPROCESS Process = PsGetCurrentProcess();
	
	// Ensure the 3 LS bits are clear. This is usually the case if 
	ASSERT(((uintptr_t) Object & 0x7) == 0);
	
	// Add a reference to this object, because it is now also referenced in the handle table.
	ObReferenceObjectByPointer(Object);
	
	OB_HANDLE_ITEM HandleItem;
	HandleItem.Pointer = 0;
	HandleItem.U.AddressBits = (uintptr_t)Object >> 3;
	
	if (OpenFlags & OB_OPEN_INHERIT)
		HandleItem.U.Inherit = 1;
	
	Status = ExCreateHandle(Process->HandleTable, HandleItem.Pointer, OutHandle);
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

BSTATUS ObInsertObject(void* Object, PHANDLE OutHandle, int OpenFlags)
{
	return ObpInsertObject(Object, OutHandle, OB_CREATE_HANDLE, OpenFlags);
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
		
		ObReferenceObjectByPointer(InitialParseObject);
		ExUnlockHandleTable(Process->HandleTable);
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
	Status = ObpInsertObject(Object, OutHandle, OB_OPEN_HANDLE, OpenFlags);

	// De-reference the object because ObReferenceObjectByName adds a reference to the object.
	ObDereferenceObject(Object);
	return Status;
}
