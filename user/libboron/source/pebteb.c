#include <boron.h>
#include <string.h>
#include "pebteb.h"

PTEB OSDLLGetCurrentTeb()
{
	if (OSDLLGetCurrentPeb()->Override.BlockTebAccess) {
		DbgPrint("OSDLL: Cannot get current TEB safely, most likely because libc is using it");
		return NULL;
	}
	
	return (PTEB) OSGetCurrentTeb();
}

PPEB OSDLLGetCurrentPeb()
{
	return (PPEB) OSGetCurrentPeb();
}

PTEB OSDLLCreateTebObject(PPEB Peb)
{
	if (!Peb)
		Peb = OSDLLGetCurrentPeb();
	
	PTEB Teb = OSAllocate(sizeof(TEB));
	if (!Teb)
		return NULL;
	
	memset(Teb, 0, sizeof *Teb);
	
	// Initialize the TEB structure.
	Teb->Peb = Peb;
	
	return Teb;
}

BSTATUS OSDLLCreateTeb(PPEB Peb)
{
	PTEB Teb = OSDLLCreateTebObject(Peb);
	if (!Teb)
		return STATUS_INSUFFICIENT_MEMORY;
	
	OSSetCurrentTeb(Teb);
	return STATUS_SUCCESS;
}
