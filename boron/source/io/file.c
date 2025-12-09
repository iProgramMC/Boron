/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/file.c
	
Abstract:
	This module implements the I/O File object type.
	
Author:
	iProgramInCpp - 28 April 2024
***/
#include "iop.h"

// Create a file object from an FCB.
BSTATUS IoCreateFileObject(PFCB Fcb, PFILE_OBJECT* OutObject, uint32_t Flags, uint32_t OpenFlags)
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
	FileObject->Mappable.Dispatch = &IopFileObjectMappableDispatch;
	FileObject->Fcb      = Fcb;
	FileObject->Flags    = Flags;
	FileObject->Context1 = NULL;
	FileObject->Context2 = NULL;
	FileObject->CurrentFileOffset = 0;
	FileObject->CurrentDirectoryVersion = 0;
	FileObject->OpenFlags = OpenFlags;
	
	KeInitializeMutex(&FileObject->FileOffsetMutex, 0);
	
	// Call the FCB's create object method, if it exists.
	IO_CREATE_OBJ_METHOD CreateObjMethod = Fcb->DispatchTable->CreateObject;
	
	if (CreateObjMethod)
		CreateObjMethod(Fcb, FileObject);
	
	IoReferenceFcb(Fcb);
	return STATUS_SUCCESS;
}

BSTATUS IopOpenFile(void* Object, UNUSED int HandleCount, UNUSED OB_OPEN_REASON OpenReason)
{
	BSTATUS Status = STATUS_SUCCESS;
	PFILE_OBJECT File = Object;
	IO_OPEN_METHOD OpenMethod = File->Fcb->DispatchTable->Open;
	
	if (OpenMethod)
		Status = OpenMethod(File->Fcb, File->OpenFlags);
	
	return Status;
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
	
	// Delete the reference to the FCB.
	IO_DELETE_OBJ_METHOD DeleteObjMethod = File->Fcb->DispatchTable->DeleteObject;
	
	if (DeleteObjMethod)
		DeleteObjMethod(File->Fcb, Object);
	
	IoDereferenceFcb(File->Fcb);
}

// IopParseFile is implemented in io/parse.c
