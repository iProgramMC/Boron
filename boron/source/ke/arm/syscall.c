/***
	The Boron Operating System
	Copyright (C) 2025-2026 iProgramInCpp

Module name:
	ke/arm/syscall.c
	
Abstract:
	This module implements the system service dispatcher
	for the arm architecture.
	
Author:
	iProgramInCpp - 30 December 2025
***/
#include <ke.h>

void KiSystemServiceHandler(PKREGISTERS Regs)
{
	(void) Regs;
	
	KeCrash("NYI KiSystemServiceHandler");
}
