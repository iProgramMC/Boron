/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/armv6/init.c
	
Abstract:
	This module implements the architecture specific UP-init
	and MP-init routines.
	
Author:
	iProgramInCpp - 29 December 2023
***/
#include <ke.h>
#include <hal.h>

void KiInitializeInterruptSystem();

INIT
void KeInitArchUP()
{
	KiInitializeInterruptSystem();
}

INIT
void KeInitArchMP()
{
	KeInitCPU();
	KeLowerIPL(IPL_NORMAL);
	HalInitSystemMP();
}