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
	(void) Flags;
	
	return IoReadFileMdl(
		Iosb,
		Session->Terminal->HostToSessionPipe,
		MdlBuffer,
		Offset,
		false
	);
}

// The session calls write, this means that it's sending information to the host.
BSTATUS TtyWriteSessionFile(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, PMDL MdlBuffer, uint32_t Flags)
{
	PREP_EXT;
	(void) Flags;
	
	return IoWriteFileMdl(
		Iosb,
		Session->Terminal->SessionToHostPipe,
		MdlBuffer,
		Offset,
		false
	);
}

IO_DISPATCH_TABLE TtySessionDispatch =
{
	.CreateObject = TtyCreateObjectSessionFile,
	.DeleteObject = TtyDeleteObjectSessionFile,
	.Read = TtyReadSessionFile,
	.Write = TtyWriteSessionFile,
	.Seekable = TtyIsSeekableSessionFile,
};
