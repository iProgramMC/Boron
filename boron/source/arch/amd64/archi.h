/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	arch/amd64/archi.h
	
Abstract:
	This header file contains the internal definitions for
	the "Architecture" component of the kernel core.
	
Author:
	iProgramInCpp - 28 October 2023
***/
#pragma once

#include <arch.h>

extern
int KiVectorCrash,
    KiVectorTlbShootdown,
    KiVectorDpcIpi;

PKREGISTERS KiHandleApcIpi(PKREGISTERS Regs);
PKREGISTERS KiHandleDpcIpi(PKREGISTERS Regs);
PKREGISTERS KiHandleCrashIpi(PKREGISTERS Regs);
PKREGISTERS KiHandleTlbShootdownIpi(PKREGISTERS Regs);
