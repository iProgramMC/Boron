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

extern POBJECT_TYPE TtyTerminalObjectType;
extern OBJECT_TYPE_INFO TtyTerminalObjectTypeInfo;
extern IO_DISPATCH_TABLE TtyHostDispatch, TtySessionDispatch;

// ttyobj.c contains a more in-depth explanation of the rationale and design
// of the pseudoterminal subsystem.

typedef struct
{
	PFILE_OBJECT HostToSessionPipe;
	PFILE_OBJECT SessionToHostPipe;
	PFCB HostToSessionPipeFcb;
	PFCB SessionToHostPipeFcb;
	
	PFCB HostFcb;
	PFCB SessionFcb;
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
