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
static BSTATUS IopCallParse(
	PFILE_OBJECT FileObject,
	const char* Path,
	int LoopCount,
	const char** OutReparsePath,
	PIO_PARSE_CONTEXT ParseContext,
	void** OutObject
)
{
	BSTATUS Status;
	PFCB Fcb = FileObject->Fcb;

	PFCB FoundFcb = NULL;
	IO_PARSE_DIR_METHOD ParseDir = Fcb->DispatchTable->ParseDir;

	IO_PARSE_CONTEXT DefaultParseContext;
	if (!ParseContext)
	{
		DefaultParseContext.FileAttributes = 0;
		DefaultParseContext.CreateOptions = IO_FILE_ANY_TYPE;
		DefaultParseContext.CreateDisposition = IO_OPEN;
		DefaultParseContext.AllocationSize = 0;
		DefaultParseContext.TargetName = NULL;
		DefaultParseContext.ResultCode = IO_FILE_NO_ACTION_TAKEN;
		ParseContext = &DefaultParseContext;
	}

	if (!ParseDir)
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

		Status = ParseDir(&Iosb, FileObject, Path, LoopCount, ParseContext);

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
		ASSERT((Iosb.ParseDir.ReparsePath == NULL || (Path <= Iosb.ParseDir.ReparsePath && Iosb.ParseDir.ReparsePath <= Path + Length)) &&
			"ParseDir is supposed to return a substring of the initial path or NULL");
#endif

		ParseContext->ResultCode = Iosb.ParseDir.ResultCode;
	}

	// Create a file object based on that file and return.
	// Note that ParseDir will have added a reference to the FCB.
	PFILE_OBJECT* OutObject2 = (PFILE_OBJECT*) OutObject;

	// NOTE: Transfers the reference to the FCB into the file object.
	Status = IoCreateFileObject(
		FoundFcb,
		OutObject2,
		ParseContext->CreateOptions
	);

	IoDereferenceFcb(FoundFcb);

	return Status;
}

BSTATUS IopParseDevice(void* Object, const char** Name, UNUSED void* Context, UNUSED int LoopCount, void** OutObject)
{
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
		BSTATUS Status = IoOpenDeviceObject(DeviceObject, OutObject2, 0);

		if (FAILED(Status))
			return Status;

		// Assign the output path name to NULL.
		*Name = NULL;
		return STATUS_SUCCESS;
	}

	// There is a path. Device objects are not directories no matter how much they say they are.
	return STATUS_NOT_A_DIRECTORY;
}

BSTATUS IopParseFile(void* Object, const char** ParsePath, void* ParseContext, int LoopCount, void** OutObject)
{
	const char* PathName = *ParsePath;

	PFILE_OBJECT FileObject = Object;

	IoLockFcbShared(FileObject->Fcb);
	return IopCallParse(FileObject, PathName, LoopCount, ParsePath, ParseContext, OutObject);
}
