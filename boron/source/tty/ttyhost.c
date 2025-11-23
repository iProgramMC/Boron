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
#include <string.h>

// Flushes the temporary buffer used to accumulate writes.
BSTATUS TtyFlushTempBuffer(PTERMINAL Terminal)
{
	if (Terminal->LineState.TempBufferLength == 0)
		return STATUS_SUCCESS;
	
	IO_STATUS_BLOCK Ignored;
	BSTATUS Status = IoWriteFile(
		&Ignored,
		Terminal->SessionToHostPipe,
		Terminal->LineState.TempBuffer,
		Terminal->LineState.TempBufferLength,
		Terminal->LineState.S2HIoFlags,
		0,     // FileOffset
		false  // Cached
	);
	
	Terminal->LineState.TempBufferLength = 0;
#ifdef SECURE
	memset(Terminal->LineState.TempBuffer, 0, sizeof Terminal->LineState.TempBuffer);
#endif
	return Status;
}

BSTATUS TtyWriteCharacterToTempBuffer(PTERMINAL Terminal, char Character, bool Escape)
{
	BSTATUS Status;
	if (Terminal->LineState.TempBufferLength >= TEMP_BUFFER_MAX)
	{
		Status = TtyFlushTempBuffer(Terminal);
		if (FAILED(Status))
			return Status;
	}
	
	if (Escape)
	{
		if (Character == 127)
		{
			Status = TtyWriteCharacterToTempBuffer(Terminal, '^', false);
			if (FAILED(Status))
				return Status;
			
			Character = '?';
		}
		else if (Character >= 0 && Character <= 31)
		{
			Status = TtyWriteCharacterToTempBuffer(Terminal, '^', false);
			if (FAILED(Status))
				return Status;
			
			Character += '@';
		}
	}
	
	Terminal->LineState.TempBuffer[Terminal->LineState.TempBufferLength++] = Character;
	return STATUS_SUCCESS;
}

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

static BSTATUS TtyProcessCharacter(PTERMINAL Terminal, char Character);

// The host calls write, this means that it's sending information to the session.
BSTATUS TtyWriteHostFile(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, PMDL MdlBuffer, uint32_t Flags)
{
	PREP_EXT;
	
	// If raw mode is enabled, echo isn't enabled, and no conversion
	// flags are set, then it's safe to just mirror the write directly.
	PTERMINAL Terminal = Host->Terminal;
	
	if (Terminal->State.Local.RawMode &&
		!Terminal->State.Local.Echo &&
		!Terminal->State.Input.IgnoreCR &&
		!Terminal->State.Input.ConvertCRToNL &&
		!Terminal->State.Input.ConvertNLToCR)
	{
		return IoWriteFileMdl(
			Iosb,
			Terminal->HostToSessionPipe,
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
	
	// TODO: what if they requested non-block IO?
	Terminal->LineState.H2SIoFlags = Flags;
	Terminal->LineState.S2HIoFlags = 0;
	char* Chars = ReadAddress;
	for (size_t i = 0; i < MdlBuffer->ByteCount; i++)
	{
		Status = TtyProcessCharacter(Terminal, Chars[i]);
		if (FAILED(Status))
		{
			Iosb->BytesWritten = i;
			Iosb->Status = Status;
			break;
		}
	}
	
	if (SUCCEEDED(Status))
	{
		Iosb->BytesWritten = MdlBuffer->ByteCount;
		Iosb->Status = Status;
	}
	
	TtyFlushTempBuffer(Terminal);
	MmUnmapPagesMdl(MdlBuffer);
	KeReleaseMutex(&Terminal->StateMutex);
	return Status;
}

// Flushes the line buffer to the host->session pipe.
static BSTATUS TtyFlushLineBuffer(PTERMINAL Terminal)
{
	if (Terminal->LineState.LineBufferLength == 0)
		return STATUS_SUCCESS;
	
	IO_STATUS_BLOCK Ignored;
	BSTATUS Status = IoWriteFile(
		&Ignored,
		Terminal->HostToSessionPipe,
		Terminal->LineState.LineBuffer,
		Terminal->LineState.LineBufferLength,
		Terminal->LineState.H2SIoFlags,
		0,     // FileOffset
		false  // Cached
	);
	if (FAILED(Status))
		return Status;
	
	Terminal->LineState.LineBufferLength = 0;
	Terminal->LineState.LineBufferPosition = 0;
#ifdef SECURE
	memset(Terminal->LineState.LineBuffer, 0, sizeof Terminal->LineState.LineBuffer);
#endif
	return STATUS_SUCCESS;
}

// Writes a string, typically an ANSI escape code, to the session->host pipe.
static BSTATUS TtyWriteStringEcho(PTERMINAL Terminal, const char* String)
{
	if (!Terminal->State.Local.Echo)
		// Echo disabled.
		return STATUS_SUCCESS;
	
	while (*String)
	{
		TtyWriteCharacterToTempBuffer(Terminal, *String, false);
		String++;
	}
	
	return STATUS_SUCCESS;
}

// Writes the current line, starting from the current position, to the session->host pipe.
static BSTATUS TtyWriteCurrentLineFromPosition(PTERMINAL Terminal, int Position)
{
	if (!Terminal->State.Local.Echo)
		// Echo disabled.
		return STATUS_SUCCESS;
	
	for (int i = Position; i < Terminal->LineState.LineBufferLength; i++)
		TtyWriteCharacterToTempBuffer(Terminal, Terminal->LineState.LineBuffer[i], true);

	return STATUS_SUCCESS;
}

// Writes a character to the host->session pipe.
static BSTATUS TtyWriteCharacter(PTERMINAL Terminal, char Character)
{
	IO_STATUS_BLOCK Ignored;
	return IoWriteFile(
		&Ignored,
		Terminal->HostToSessionPipe,
		&Character,
		1,     // Length
		Terminal->LineState.H2SIoFlags,
		0,     // FileOffset
		false  // Cached
	);
}

// Counts the amount of visible characters (i.e. on-screen characters) are found inside
// of a range in the line buffer.  Normal characters are 1 on-screen character wide, while
// control characters are 2 on-screen characters wide.
static int TtyCountVisibleCharacters(PTERMINAL Terminal, int Start, int Length)
{
	int Count = Length;
	
	for (int i = Start; i < Start + Length; i++)
	{
		char Char = Terminal->LineState.LineBuffer[i];
		if ((Char >= 0 && Char <= 31) || Char == 127)
			Count++;
	}
	
	return Count;
}

static BSTATUS TtyProcessCharacter(PTERMINAL Terminal, char Character)
{
	BSTATUS Status;
	if (Terminal->State.Input.IgnoreCR && Character == '\r')
		return STATUS_SUCCESS;
	
	if (Terminal->State.Input.ConvertCRToNL && Character == '\r')
		Character = '\n';
	else if (Terminal->State.Input.ConvertNLToCR && Character == '\n')
		Character = '\r';
	
	if (Terminal->State.Local.RawMode)
	{
		// Not canonical mode, simply pass it along.
		return TtyWriteCharacter(Terminal, Character);
	}
	
	if (Character == Terminal->State.Chars.Erase)
	{
		// Backspace.  Erase the character at the current position.
		if (Terminal->LineState.LineBufferPosition == 0)
			return STATUS_SUCCESS;
		
		// This is scary.
		memmove(
			Terminal->LineState.LineBuffer + Terminal->LineState.LineBufferPosition,
			Terminal->LineState.LineBuffer + Terminal->LineState.LineBufferPosition + 1,
			sizeof(Terminal->LineState.LineBuffer) - Terminal->LineState.LineBufferPosition - 1
		);
		Terminal->LineState.LineBufferLength--;
		Terminal->LineState.LineBufferPosition--;
		
		if (Terminal->State.Local.Echo)
		{
			char Buffer[16];
			snprintf(Buffer, sizeof Buffer, "\x1B[%dD\x1B[K", TtyCountVisibleCharacters(Terminal, Terminal->LineState.LineBufferPosition, 1));
			TtyWriteStringEcho(Terminal, Buffer);
			TtyWriteCurrentLineFromPosition(Terminal, Terminal->LineState.LineBufferPosition);
		}
		
		return STATUS_SUCCESS;
	}
	
	if (Character == Terminal->State.Chars.EraseLine)
	{
		// Go back the required amount of characters, then erase the entire line.
		char Buffer[16];
		snprintf(Buffer, sizeof Buffer, "\x1B[%dD\x1B[K", TtyCountVisibleCharacters(Terminal, 0, Terminal->LineState.LineBufferPosition));
		TtyWriteStringEcho(Terminal, Buffer);
		
		Terminal->LineState.LineBufferLength = 0;
		Terminal->LineState.LineBufferPosition = 0;
#ifdef SECURE
		memset(Terminal->LineState.LineBuffer, 0, sizeof Terminal->LineState.LineBuffer);
#endif
		return STATUS_SUCCESS;
	}
	
	if (Character == Terminal->State.Chars.Interrupt)
	{
		// TODO: Add signals
		DbgPrint("TTY: Interrupt pressed!");
		return STATUS_SUCCESS;
	}
	
	if (Character == Terminal->State.Chars.Suspend)
	{
		// TODO: Add job control
		DbgPrint("TTY: Suspend pressed!");
		return STATUS_SUCCESS;
	}
	
	if (Character == '\r' ||
		Character == '\n' ||
		Character == Terminal->State.Chars.EndOfLine ||
		Character == Terminal->State.Chars.EndOfLine2)
	{
		if (Terminal->State.Local.Echo)
			TtyWriteCharacterToTempBuffer(Terminal, Character, false);
		
		TtyFlushTempBuffer(Terminal);
		
		Status = TtyFlushLineBuffer(Terminal);
		TtyWriteCharacter(Terminal, Character);
		return Status;
	}
	
	if (Character == Terminal->State.Chars.EndOfFile)
	{
		// The end of file character is not added to the buffer.
		// The line is submitted without a newline added to it.
		Terminal->LineState.H2SIoFlags |= IO_RW_TERMINATE_READ;
		TtyFlushLineBuffer(Terminal);
		return STATUS_END_OF_FILE;
	}
	
	// Normal character.
	if (Terminal->LineState.LineBufferLength >= LINE_BUFFER_MAX)
		return TtyFlushLineBuffer(Terminal);
	
	// This is also scary.
	memmove(
		Terminal->LineState.LineBuffer + Terminal->LineState.LineBufferPosition + 1,
		Terminal->LineState.LineBuffer + Terminal->LineState.LineBufferPosition,
		sizeof(Terminal->LineState.LineBuffer) - Terminal->LineState.LineBufferPosition - 1
	);
	
	Terminal->LineState.LineBuffer[Terminal->LineState.LineBufferPosition] = Character;
	Terminal->LineState.LineBufferLength++;
	TtyWriteCurrentLineFromPosition(Terminal, Terminal->LineState.LineBufferPosition);
	Terminal->LineState.LineBufferPosition++;
	return STATUS_SUCCESS;
}

BSTATUS TtyIoControlHostFile(PFCB Fcb, int IoControlCode, const void* InBuffer, size_t InBufferSize, void* OutBuffer, size_t OutBufferSize)
{
	PREP_EXT;
	return TtyDeviceIoControl(Host->Terminal, IoControlCode, InBuffer, InBufferSize, OutBuffer, OutBufferSize);
}

IO_DISPATCH_TABLE TtyHostDispatch =
{
	.CreateObject = TtyCreateObjectHostFile,
	.DeleteObject = TtyDeleteObjectHostFile,
	.Read = TtyReadHostFile,
	.Write = TtyWriteHostFile,
	.Seekable = TtyIsSeekableHostFile,
	.IoControl = TtyIoControlHostFile,
};
