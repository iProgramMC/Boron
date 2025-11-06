/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ttyhost.c
	
Abstract:
	This module implements the TTY (Virtual Teletype)
	host file's support routines.
	
Author:
	iProgramInCpp - 30 October 2025
***/
#include "ttyi.h"

#define PREP_EXT PTERMINAL_HOST Host = (PTERMINAL_HOST) Fcb->Extension

bool TtyIsSeekableHostFile(UNUSED PFCB Fcb)
{
	return false;
}

void TtyCreateObjectHostFile(PFCB Fcb, UNUSED PFILE_OBJECT FileObject)
{
	PREP_EXT;
	
	ObReferenceObjectByPointer(Host->Terminal);
}

void TtyDeleteObjectHostFile(PFCB Fcb, UNUSED PFILE_OBJECT FileObject)
{
	PREP_EXT;
	
	ObDereferenceObject(Host->Terminal);
}

// The host calls read, this means that it's receiving information from the session.
BSTATUS TtyReadHostFile(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, PMDL MdlBuffer, uint32_t Flags)
{
	PREP_EXT;
	
	return IoReadFileMdl(
		Iosb,
		Host->Terminal->SessionToHostPipe,
		MdlBuffer,
		Flags,
		Offset,
		false
	);
}

// The host calls write, this means that it's sending information to the session.
BSTATUS TtyWriteHostFile(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, PMDL MdlBuffer, uint32_t Flags)
{
	PREP_EXT;
	
	return IoWriteFileMdl(
		Iosb,
		Host->Terminal->HostToSessionPipe,
		MdlBuffer,
		Flags,
		Offset,
		false
	);
}

IO_DISPATCH_TABLE TtyHostDispatch =
{
	.CreateObject = TtyCreateObjectHostFile,
	.DeleteObject = TtyDeleteObjectHostFile,
	.Read = TtyReadHostFile,
	.Write = TtyWriteHostFile,
	.Seekable = TtyIsSeekableHostFile,
};
