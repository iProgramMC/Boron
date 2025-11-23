/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ttysess.c
	
Abstract:
	This module implements the TTY (Virtual Teletype)
	session file's support routines.
	
Author:
	iProgramInCpp - 30 October 2025
***/
#include "ttyi.h"

#define PREP_EXT PTERMINAL_SESSION Session = (PTERMINAL_SESSION) Fcb->Extension

bool TtyIsSeekableSessionFile(UNUSED PFCB Fcb)
{
	return false;
}

void TtyCreateObjectSessionFile(PFCB Fcb, UNUSED PFILE_OBJECT FileObject)
{
	PREP_EXT;
	
	ObReferenceObjectByPointer(Session->Terminal);
}

void TtyDeleteObjectSessionFile(PFCB Fcb, UNUSED PFILE_OBJECT FileObject)
{
	PREP_EXT;
	
	ObDereferenceObject(Session->Terminal);
}

// The session calls read, this means that it's receiving information from the host.
BSTATUS TtyReadSessionFile(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, PMDL MdlBuffer, uint32_t Flags)
{
	PREP_EXT;
	
	return IoReadFileMdl(
		Iosb,
		Session->Terminal->HostToSessionPipe,
		MdlBuffer,
		Flags,
		Offset,
		false
	);
}

// The session calls write, this means that it's sending information to the host.
BSTATUS TtyWriteSessionFile(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, PMDL MdlBuffer, uint32_t Flags)
{
	PREP_EXT;
	PTERMINAL Terminal = Session->Terminal;
	
	if (!Terminal->State.Output.ConvertNLToCRNL &&
		!Terminal->State.Output.ConvertCRToNLOutput)
	{
		// Simple pass through.
		return IoWriteFileMdl(
			Iosb,
			Session->Terminal->SessionToHostPipe,
			MdlBuffer,
			Flags,
			Offset,
			false
		);
	}
	
	BSTATUS Status = KeWaitForSingleObject(&Terminal->StateMutex, true, TIMEOUT_INFINITE, KeGetPreviousMode());
	if (FAILED(Status))
	{
		Iosb->Status = Status;
		return Status;
	}
	
	void* ReadAddress = NULL;
	Status = MmMapPinnedPagesMdl(MdlBuffer, &ReadAddress);
	if (FAILED(Status))
	{
		Iosb->Status = Status;
		KeReleaseMutex(&Terminal->StateMutex);
		return Status;
	}
	
	Terminal->LineState.H2SIoFlags = 0;
	Terminal->LineState.S2HIoFlags = Flags;
	char* Chars = ReadAddress;
	size_t BytesWritten = 0;
	for (size_t i = 0; i < MdlBuffer->ByteCount; i++)
	{
		char Char = Chars[i];
		if (Terminal->State.Output.ConvertCRToNLOutput && Char == '\r')
			Char = '\n';
		
		if (Terminal->State.Output.ConvertNLToCRNL && Char == '\n')
		{
			Status = TtyWriteCharacterToTempBuffer(Terminal, '\r', false);
			if (FAILED(Status))
				break;
		}
		
		Status = TtyWriteCharacterToTempBuffer(Terminal, Char, false);
		if (FAILED(Status))
			break;
		
		BytesWritten = i;
	}
	
	Iosb->BytesWritten = BytesWritten;
	Iosb->Status = Status;
	TtyFlushTempBuffer(Terminal);
	MmUnmapPagesMdl(MdlBuffer);
	KeReleaseMutex(&Terminal->StateMutex);
	return Status;
}

BSTATUS TtyIoControlSessionFile(PFCB Fcb, int IoControlCode, const void* InBuffer, size_t InBufferSize, void* OutBuffer, size_t OutBufferSize)
{
	PREP_EXT;
	return TtyDeviceIoControl(Session->Terminal, IoControlCode, InBuffer, InBufferSize, OutBuffer, OutBufferSize);
}

IO_DISPATCH_TABLE TtySessionDispatch =
{
	.CreateObject = TtyCreateObjectSessionFile,
	.DeleteObject = TtyDeleteObjectSessionFile,
	.Read = TtyReadSessionFile,
	.Write = TtyWriteSessionFile,
	.Seekable = TtyIsSeekableSessionFile,
	.IoControl = TtyIoControlSessionFile,
};
