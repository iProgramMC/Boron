/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/i386/init.c
	
Abstract:
	This module implements the architecture specific UP-init
	routines.
	
Author:
	iProgramInCpp - 14 October 2025
***/
#include <ke.h>
#include <hal.h>

void KiSetupIdt();
void KiInitializeInterruptSystem();

INIT
void KeInitArchUP()
{
	KiSetupIdt();
	KiInitializeInterruptSystem();
}

INIT
void KeInitArchMP()
{
	KeInitCPU();
	KeLowerIPL(IPL_NORMAL);
	HalInitSystemMP();
}
