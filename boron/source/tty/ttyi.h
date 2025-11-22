/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ttyi.h
	
Abstract:
	This header defines internal function prototypes for
	the pseudoterminal subsystem.
	
Author:
	iProgramInCpp - 30 October 2025
***/
#pragma once

#include <io.h>
#include <ob.h>
#include <ttys.h>

extern POBJECT_TYPE TtyTerminalObjectType;
extern OBJECT_TYPE_INFO TtyTerminalObjectTypeInfo;
extern IO_DISPATCH_TABLE TtyHostDispatch, TtySessionDispatch;

// ttyobj.c contains a more in-depth explanation of the rationale and design
// of the pseudoterminal subsystem.

#define LINE_BUFFER_MAX (4096)
#define TEMP_BUFFER_MAX (32)
#define ESCAPE_BUFFER_MAX (16)

typedef struct
{
	PFILE_OBJECT HostToSessionPipe;
	PFILE_OBJECT SessionToHostPipe;
	PFCB HostToSessionPipeFcb;
	PFCB SessionToHostPipeFcb;
	
	PFCB HostFcb;
	PFCB SessionFcb;
	
	KMUTEX StateMutex;
	TERMINAL_STATE State;
	
	struct
	{
		char LineBuffer[LINE_BUFFER_MAX];
		
		// The editing position inside the line buffer.
		uint16_t LineBufferPosition;
		
		// The length of the contents of the line buffer.
		uint16_t LineBufferLength;
		
		char TempBuffer[TEMP_BUFFER_MAX];
		
		// The length of the contents of the temporary buffer.
		// This buffer is built during a write operation from the terminal
		// host to the session, and at the end (or when the temp buffer is
		// full), the contents of this buffer are flushed to the session
		// to host stream.
		uint16_t TempBufferLength;
		
		char EscapeBuffer[ESCAPE_BUFFER_MAX];
		
		// The length of the contents of the escape character buffer.
		// This buffer is built during a write operation.
		uint16_t EscapeBufferLength;
		
		// If this byte is set, an escape character was received, and
		// currently an escape character is being read.
		bool EscapeMode;
		
		// Only used while processing terminal input. Flags used during
		// I/O operations from host to session.
		uint32_t H2SIoFlags;
	}
	LineState;
	
	struct
	{
		int Width;
		int Height;
	}
	Window;
}
TERMINAL, *PTERMINAL;

typedef struct
{
	PTERMINAL Terminal;
	
	int Dummy;
}
TERMINAL_HOST, *PTERMINAL_HOST;

typedef struct
{
	PTERMINAL Terminal;
	
	int Dummy;
}
TERMINAL_SESSION, *PTERMINAL_SESSION;

// Terminal object dispatch functions.
void TtyDeleteTerminal(void* ObjectV);

// These are for use with ExCreateObjectUserCall
typedef struct
{
	size_t BufferSize;
}
TERMINAL_INIT_CONTEXT, *PTERMINAL_INIT_CONTEXT;

BSTATUS TtyInitializeTerminal(void* TerminalV, void* Context);
