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

// Checks if this file is seekable.
bool IoIsSeekable(PFCB Fcb)
{
	IO_SEEKABLE_METHOD Seekable = Fcb->DispatchTable->Seekable;
	
	if (!Seekable)
		return false;
	
	return Seekable(Fcb);
}

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
	FileObject->Fcb      = Fcb;
	FileObject->Flags    = Flags;
	FileObject->Context1 = NULL;
	FileObject->Context2 = NULL;
	FileObject->DirectoryOffset  = 0;
	FileObject->DirectoryVersion = 0;
	FileObject->OpenFlags = OpenFlags;
	
	// Call the FCB's create object method, if it exists.
	//
	// This should add a reference to the FCB through the file system driver
	// (FSD) -- the kernel does not actually manage FCBs' reference counts!
	IO_CREATE_OBJ_METHOD CreateObjMethod = Fcb->DispatchTable->CreateObject;
	
	if (CreateObjMethod)
		CreateObjMethod(Fcb, FileObject);
	
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

void* IopDuplicateFile(void* Object, UNUSED int OpenReason)
{
	DbgPrint("IopDuplicateFile(%p)", Object);
	PFILE_OBJECT File = Object;
	
	// Increase the FCB's reference count by one.
	IoReferenceFcb(File->Fcb);
	
	// Create a new file object to which this reference is transferred.
	PFILE_OBJECT NewFile = NULL;
	BSTATUS Status = IoCreateFileObject(File->Fcb, &NewFile, File->Flags, File->OpenFlags);
	
	if (FAILED(Status))
	{
		DbgPrint(
			"ERROR: IopDuplicateFile failed with status %d (%s). The file object won't be duplicated. "
			"This behavior might not be desirable later.",
			Status,
			RtlGetStatusString(Status)
		);
		
		return NULL;
	}
	
	return NewFile;
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
