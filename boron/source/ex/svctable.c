/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ex/svctable.c
	
Abstract:
	This module defines the system service table for Boron.
	
Author:
	iProgramInCpp - 22 April 2025
***/
#include <mm.h>
#include <io.h>
#include <ps.h>
#include <ex.h>

BSTATUS OSDummy()
{
	DbgPrint("Dummy system call invoked\n");
	LogMsg("Dummy system call invoked\n");
	return 0;
}

// KEEP IN SYNC with ke/amd64/syscall.asm !!!

const void* const KiSystemServices[] =
{
	OSExitThread,
	OSDummy,
};
