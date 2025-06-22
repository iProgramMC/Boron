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

// NOTE: Unlocks the FCB when done.
static BSTATUS IopCallParse(PFCB Fcb, const char* Path, int LoopCount, const char** OutReparsePath, void** OutObject)
{
	BSTATUS Status;
	PFCB FoundFcb = NULL;
	IO_PARSE_DIR_METHOD Parse = Fcb->DispatchTable->ParseDir;
	
	if (!Parse)
	{
		if (Fcb->DispatchTable->Flags & DISPATCH_FLAG_DIRECTLY_OPENABLE)
		{
			// This file cannot be parsed as a directory but it's directly openable
			// so let's do that.  This is only used by the init root file system.
			FoundFcb = Fcb;
			*OutReparsePath = NULL;
		}
		else
		{
			IoUnlockFcb(Fcb);
			return STATUS_NOT_A_DIRECTORY;
		}
	}
	
	if (!FoundFcb)
	{
		// There is a possibility that this file is parsable, so let's do that.
		IO_STATUS_BLOCK Iosb;
		
	#ifdef DEBUG
		memset (&Iosb, 0, sizeof Iosb);
	#endif
		
		Status = Parse(&Iosb, Fcb, Path, LoopCount);
		
		IoUnlockFcb(Fcb);
		
		if (FAILED(Status))
			return Status;
		
		FoundFcb = Iosb.ParseDir.FoundFcb;
		ASSERT(FoundFcb && "ParseDir is supposed to return an FCB if it returned a success value");
		
		ASSERT(Status == Iosb.Status && "ParseDir is supposed to return an identical status in its IOSB");
		
		*OutReparsePath = Iosb.ParseDir.ReparsePath;
		
		// Ensure that it's a substring of the current path in debug mode.
#ifdef DEBUG
		size_t Length = strlen (Path);
		ASSERT((Iosb.ParseDir.ReparsePath == NULL || (Path <= Iosb.ParseDir.ReparsePath && Iosb.ParseDir.ReparsePath < Path + Length)) &&
			"ParseDir is supposed to return a substring of the initial path or NULL");
#endif
	}
	
	// Create a file object based on that file and return.
	// Note that ParseDir will have added a reference to the FCB.
	PFILE_OBJECT* OutObject2 = (PFILE_OBJECT*) OutObject;
	
	Status = IoCreateFileObject(
		FoundFcb,
		OutObject2,
		0,
		0
	);
	
	// Remove this function's reference.
	// IoCreateFileObject creates a reference of its own.
	IoDereferenceFcb(FoundFcb);
	
	return Status;
}

BSTATUS IopParseDevice(void* Object, const char** Name, UNUSED void* Context, UNUSED int LoopCount, void** OutObject)
{
	BSTATUS Status;
	
	// TODO: We no longer directly use device objects for mounting, so lots of this code is useless.
	
	if (ObGetObjectType(Object) != IoDeviceType)
		return STATUS_TYPE_MISMATCH;
	
	if (!Name || !*Name)
		return STATUS_INVALID_PARAMETER;
	
	PDEVICE_OBJECT DeviceObject = Object;
	
	const char* Path = *Name;
	if (!*Path)
	{
		// If the path is completely empty, we are opening the device itself.
		// Create a file object for the device.
		
		PFILE_OBJECT* OutObject2 = (PFILE_OBJECT*) OutObject;
		BSTATUS Status = IoOpenDeviceObject(DeviceObject, OutObject2, 0, 0);
		
		if (FAILED(Status))
			return Status;
		
		// Assign the output path name to NULL.
		*Name = NULL;
		return STATUS_SUCCESS;
	}
	
	// There is a path.  Call the mounted file system (if any) to resolve it. TODO
	PFCB Fcb = DeviceObject->Fcb;
	
	Status = IoLockFcbShared(Fcb);
	
	if (FAILED(Status))
		return Status;
	
	if (Fcb->FileType == FILE_TYPE_DIRECTORY)
	{
		return IopCallParse(Fcb, Path, LoopCount, Name, OutObject);
	}
	
	// This is unsupported for a parse operation.
	IoUnlockFcb(Fcb);
	return STATUS_NOT_A_DIRECTORY;
}

BSTATUS IopParseFile(void* Object, const char** ParsePath, UNUSED void* Context, int LoopCount, void** OutObject)
{
	const char* PathName = *ParsePath;
	
	PFILE_OBJECT FileObject = Object;
	
	IoLockFcbShared(FileObject->Fcb);
	return IopCallParse(FileObject->Fcb, PathName, LoopCount, ParsePath, OutObject);
}
