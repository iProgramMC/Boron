/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ps/attach.c
	
Abstract:
	This module implements the process attachment mechanism.
	
Author:
	iProgramInCpp - 14 April 2025
***/
#include "psp.h"

void PsAttachToProcess(PEPROCESS Process)
{
	KeAttachToProcess(&Process->Pcb);
}

void PsDetachFromProcess()
{
	KeDetachFromProcess();
}

PEPROCESS PsGetAttachedProcess()
{
	PEPROCESS Process = (PEPROCESS) KeGetCurrentThread()->AttachedProcess;
	
	if (!Process)
		Process = PsGetCurrentProcess();
	
	return Process;
}
