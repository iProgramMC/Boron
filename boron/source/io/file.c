/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/device.c
	
Abstract:
	This module implements the I/O File object type.
	
Author:
	iProgramInCpp - 28 April 2024
***/
#include "iop.h"

// Create a file object. This doesn't actually open the object.
BSTATUS IopCreateFileObject(PFCB Fcb, PFILE_OBJECT* OutObject, uint32_t Flags, uint32_t OpenFlags)
{
	BSTATUS Status;
	
	PFILE_OBJECT FileObject;
	void* Object;
	Status = ObCreateObject(
		&Object,
		NULL,
		IoFileType,
		NULL,
		OB_FLAG_NO_DIRECTORY,
		NULL,
		sizeof(FILE_OBJECT)
	);
	
	if (FAILED(Status))
		return Status;
	
	FileObject = Object;
	*OutObject = Object;
	
	// Initialize the file object.
	FileObject->Fcb      = Fcb;
	FileObject->Flags    = Flags;
	FileObject->Offset   = 0;
	FileObject->Context1 = NULL;
	FileObject->Context2 = NULL;
	FileObject->OpenFlags = OpenFlags;
	
	// Call the FCB's create object method, if it exists.
	IO_CREATE_OBJ_METHOD CreateObjMethod = Fcb->DispatchTable->CreateObject;
	
	if (CreateObjMethod)
		CreateObjMethod(Fcb, FileObject);
	
	return STATUS_SUCCESS;
}

void IopOpenFile(void* Object, UNUSED int HandleCount, UNUSED OB_OPEN_REASON OpenReason)
{
	PFILE_OBJECT File = Object;
	IO_OPEN_METHOD OpenMethod = File->Fcb->DispatchTable->Open;
	
	if (OpenMethod)
	{
		BSTATUS Status = OpenMethod(File->Fcb, File->OpenFlags);
		
	#ifdef DEBUG
		if (FAILED(Status))
			KeCrash("IopOpenFile WARNING: FCB open routine returned status %d on object %p", Status, Object);
	#endif
	}
}

void IopCloseFile(void* Object, int HandleCount)
{
	PFILE_OBJECT File = Object;
	IO_CLOSE_METHOD CloseMethod = File->Fcb->DispatchTable->Close;
	
	if (CloseMethod)
	{
		BSTATUS Status = CloseMethod(File->Fcb, HandleCount);
		
	#ifdef DEBUG
		if (FAILED(Status))
			KeCrash("IopCloseFile WARNING: FCB close routine returned status %d on object %p", Status, Object);
	#endif
	}
}

void IopDeleteFile(void* Object)
{
	PFILE_OBJECT File = Object;
	IO_DELETE_OBJ_METHOD DeleteObjectMethod = File->Fcb->DispatchTable->DeleteObject;
	
	if (DeleteObjectMethod)
		DeleteObjectMethod(File->Fcb, Object);
}

// IopParseFile is implemented in io/parse.c
