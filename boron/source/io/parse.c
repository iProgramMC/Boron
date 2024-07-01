/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/parse.c
	
Abstract:
	This module implements the parse function for I/O
	device objects and file objects.
	
Author:
	iProgramInCpp - 30 June 2024
***/
#include "iop.h"

BSTATUS IopParseDevice(void* Object, const char** Name, UNUSED void* Context, UNUSED int LoopCount, void** OutObject)
{
	if (ObGetObjectType(Object) != IoDeviceType)
		return STATUS_TYPE_MISMATCH;
	
	if (!Name || !*Name)
		return STATUS_INVALID_PARAMETER;
	
	PDEVICE_OBJECT DeviceObject = Object;
	
	//PFCB Fcb = DeviceObject->Fcb;
	
	const char* Path = *Name;
	if (!*Path)
	{
		// If the path is completely empty, we are opening the device itself.
		// Create a file object for the device.
		
		PFILE_OBJECT* OutObject2 = (PFILE_OBJECT*) OutObject;
		BSTATUS Status = IopCreateDeviceFileObject(DeviceObject, OutObject2, 0, 0);
		
		if (FAILED(Status))
			return Status;
		
		// Assign the output path name to NULL.
		*Name = NULL;
		return STATUS_SUCCESS;
	}
	
	// There is a path.  Call the mounted file system (if any) to resolve it. TODO
	DbgPrint("UNIMPLEMENTED: IopParseDevice(%p, %s, %p, %d, %p)", Object, Path, Context, LoopCount, OutObject);
	return STATUS_UNIMPLEMENTED;
}

BSTATUS IopParseFile(void* Object, const char** ParsePath, UNUSED void* Context, UNUSED int LoopCount, UNUSED void** OutObject)
{
	const char* PathName = *ParsePath;
	
	// TODO
	DbgPrint("UNIMPLEMENTED: IopParseFile(%p, %s, %p, %d, %p)", Object, PathName, Context, LoopCount, OutObject);
	return STATUS_UNIMPLEMENTED;
}
