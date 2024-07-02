/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/rdwr.c
	
Abstract:
	This module implements the read and write functions
	for the I/O manager.
	
Author:
	iProgramInCpp - 1 July 2024
***/
#include "iop.h"

BSTATUS IoReadFile(
	PIO_STATUS_BLOCK Iosb,
	HANDLE Handle,
	void* Buffer,
	size_t Length,
	bool CanBlock
)
{
	BSTATUS Status;
	
	// Resolve the handle.
	void* FileObjectV;
	Status = ObReferenceObjectByHandle(Handle, &FileObjectV);
	if (FAILED(Status))
		return (Iosb->Status = Status);
	
	PFILE_OBJECT FileObject = FileObjectV;
	
	ObDereferenceObject(FileObject);
	return STATUS_UNIMPLEMENTED;
}
