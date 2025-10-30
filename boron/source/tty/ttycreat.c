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

typedef struct
{
	size_t BufferSize;
}
TERMINAL_INIT_CONTEXT, *PTERMINAL_INIT_CONTEXT;

BSTATUS TtyInitializeTerminal(void* TerminalV, void* Context)
{
	BSTATUS Status;
	PTERMINAL Terminal = TerminalV;
	PTERMINAL_INIT_CONTEXT InitContext = Context;
	
	Status = IoCreatePipeObject(&Terminal->HostToSessionPipe, &Terminal->HostToSessionPipeFcb, NULL, InitContext->BufferSize, true);
	if (FAILED(Status))
		return Status;
	
	Status = IoCreatePipeObject(&Terminal->SessionToHostPipe, &Terminal->SessionToHostPipeFcb, NULL, InitContext->BufferSize, true);
	if (FAILED(Status))
		goto Fail1;
	
	Terminal->HostFcb = IoAllocateFcb(&TtyHostDispatch, sizeof(TERMINAL_HOST), false);
	if (!Terminal->HostFcb)
	{
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto Fail2;
	}
	
	Terminal->SessionFcb = IoAllocateFcb(&TtySessionDispatch, sizeof(TERMINAL_SESSION), false);
	if (!Terminal->SessionFcb)
	{
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto Fail3;
	}
	
	// Creation succeeded, now initialize everything.
	PTERMINAL_HOST Host = (PTERMINAL_HOST) Terminal->HostFcb->Extension;
	PTERMINAL_HOST Session = (PTERMINAL_HOST) Terminal->SessionFcb->Extension;
	Host->Terminal = Terminal;
	Session->Terminal = Terminal;
	
Fail3:
	IoFreeFcb(Terminal->HostFcb);
Fail2:
	ObDereferenceObject(Terminal->SessionToHostPipe);
	IoFreeFcb(Terminal->HostToSessionPipeFcb);
Fail1:
	ObDereferenceObject(Terminal->HostToSessionPipe);
	IoFreeFcb(Terminal->SessionToHostPipeFcb);
	return Status;
}
