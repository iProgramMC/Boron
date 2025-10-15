/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/i386/ipl.c
	
Abstract:
	This header file implements support for the interrupt
	priority system.
	
Author:
	iProgramInCpp - 14 October 2025
***/
#include <ke.h>

void KeOnUpdateIPL(KIPL NewIpl, KIPL OldIpl)
{
	// TODO: lazy IPL - only program the interrupt controller if needed
	
	if (NewIpl == OldIpl)
		return;
	
	// TODO: placeholder method here
}
