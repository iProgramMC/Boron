/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/ipl.c
	
Abstract:
	This module implements the interrupt priority level
	(IPL) setter and getter.
	
Author:
	iProgramInCpp - 15 September 2023
***/

#include <ke.h>
#include <hal.h>
#include <string.h>

/*void SLogMsg2(const char* msg, ...)
{
	va_list va;
	va_start(va, msg);
	char buffer[512]; // should be big enough...
	buffer[sizeof buffer - 1] = 0;
	int chars = vsnprintf(buffer, sizeof buffer - 1, msg, va);
	strcpy(buffer + chars, "\n");
	va_end(va);
	
	// this one goes to the screen
	HalPrintStringDebug(buffer);
}*/

KIPL KeGetIPL()
{
	KPRCB* me = KeGetCurrentPRCB();
	return me->Ipl;
}

KIPL KeRaiseIPL(KIPL newIPL)
{
	KPRCB* me = KeGetCurrentPRCB();
	KIPL oldIPL = me->Ipl;
	//SLogMsg("KeRaiseIPL(%d), old %d, CPU %d, called from %p", newIPL, oldIPL, me->LapicId, __builtin_return_address(0));
	
	if (oldIPL == newIPL)
		return oldIPL; // no changes
	
	if (oldIPL > newIPL)
		KeCrash("Error, can't raise the IPL to a lower level than we are (old %d, new %d).  RA: %p", oldIPL, newIPL, CallerAddress());
	
	KeOnUpdateIPL(newIPL, oldIPL);
	
	// Set the current IPL
	me->Ipl = newIPL;
	return oldIPL;
}

// similar logic, except we will also call DPCs if needed
KIPL KeLowerIPL(KIPL newIPL)
{
	KPRCB* me = KeGetCurrentPRCB();
	KIPL oldIPL = me->Ipl;
	//SLogMsg("KeLowerIPL(%d), old %d, CPU %d, called from %p", newIPL, oldIPL, me->LapicId, __builtin_return_address(0));
	
	if (oldIPL == newIPL)
		return oldIPL; // no changes
	
	if (oldIPL < newIPL)
		KeCrash("Error, can't lower the IPL to a higher level than we are (old %d, new %d).  RA: %p", oldIPL, newIPL, CallerAddress());
	
	// Set the current IPL
	me->Ipl = newIPL;
	
	KeOnUpdateIPL(newIPL, oldIPL);
	
	// TODO (updated): Issue a self-IPI to call DPCs.
	// TODO: call DPCs
	
	return oldIPL;
}
