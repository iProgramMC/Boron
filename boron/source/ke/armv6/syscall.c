/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/armv6/syscall.c
	
Abstract:
	This module implements the system service dispatcher
	for the armv6 architecture.
	
Author:
	iProgramInCpp - 30 December 2025
***/
#include <ke.h>

void KiSystemServiceHandler(PKREGISTERS Regs)
{
	(void) Regs;
	
	KeCrash("NYI KiSystemServiceHandler");
}
