/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ttyobj.c
	
Abstract:
	This module implements the TTY (Virtual Teletype)
	object's creation routines.
	
Author:
	iProgramInCpp - 30 October 2025
***/
#include "ttyi.h"
#include <ex.h>

BSTATUS OSCreateTerminal(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes, size_t BufferSize)
{
	TERMINAL_INIT_CONTEXT InitContext;
	InitContext.BufferSize = BufferSize;
	
	return ExCreateObjectUserCall(
		OutHandle,
		ObjectAttributes,
		TtyTerminalObjectType,
		sizeof(TERMINAL),
		TtyInitializeTerminal,
		&InitContext,
		0,
		false
	);
}

BSTATUS OSCreateTerminalIoHandles(PHANDLE OutHostHandle, PHANDLE OutSessionHandle, HANDLE TerminalHandle)
{
	if (!OutHostHandle && !OutSessionHandle)
		return STATUS_INVALID_PARAMETER;
	
	BSTATUS Status;
	void* TerminalV;
	
	Status = ObReferenceObjectByHandle(TerminalHandle, TtyTerminalObjectType, &TerminalV);
	if (FAILED(Status))
		return Status;
	
	PTERMINAL Terminal = TerminalV;
	HANDLE HostHandle = HANDLE_NONE, SessionHandle = HANDLE_NONE;
	
	if (OutHostHandle)
	{
		PFILE_OBJECT FileObject = NULL;
		Status = IoCreateFileObject(Terminal->HostFcb, &FileObject, 0, 0);
		if (FAILED(Status))
			goto Fail;
		
		Status = ObInsertObject(FileObject, &HostHandle, 0);
		if (FAILED(Status))
		{
			ObDereferenceObject(FileObject);
			goto Fail;
		}
	}
	
	if (OutSessionHandle)
	{
		PFILE_OBJECT FileObject = NULL;
		Status = IoCreateFileObject(Terminal->SessionFcb, &FileObject, 0, 0);
		if (FAILED(Status))
			goto Fail;
		
		Status = ObInsertObject(FileObject, &SessionHandle, 0);
		if (FAILED(Status))
		{
			ObDereferenceObject(FileObject);
			goto Fail;
		}
	}
	
	Status = MmSafeCopy(OutHostHandle, &HostHandle, sizeof(HANDLE), KeGetPreviousMode(), true);
	if (FAILED(Status))
		goto Fail;
	
	Status = MmSafeCopy(OutSessionHandle, &SessionHandle, sizeof(HANDLE), KeGetPreviousMode(), true);
	if (SUCCEEDED(Status))
		goto Success;
	
Fail:
	if (HostHandle)
		OSClose(HostHandle);
	
	if (SessionHandle)
		OSClose(SessionHandle);
	
Success:
	ObDereferenceObject(Terminal);
	return Status;
}
