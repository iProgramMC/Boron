/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ttyinit.c
	
Abstract:
	This header defines the system services for the
	pseudoterminal subsystem.
	
Author:
	iProgramInCpp - 30 October 2025
***/
#pragma once

#include <main.h>

#ifdef KERNEL
bool TtyInitSystem();
#endif

BSTATUS OSCreateTerminal(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes, size_t BufferSize);

BSTATUS OSCreateTerminalIoHandles(PHANDLE OutHostHandle, PHANDLE OutSessionHandle, HANDLE TerminalHandle);

BSTATUS OSCheckIsTerminalFile(HANDLE FileHandle);
