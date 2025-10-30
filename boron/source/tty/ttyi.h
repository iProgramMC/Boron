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

extern POBJECT_TYPE TtyTerminalObjectType;
extern OBJECT_TYPE_INFO TtyTerminalObjectTypeInfo;
extern IO_DISPATCH_TABLE TtyHostDispatch, TtySessionDispatch;

void TtyDeleteTerminal(void* ObjectV);
