#include <boron.h>
#include <string.h>
#include "pebteb.h"

PTEB OSDLLGetCurrentTeb()
{
	return (PTEB) OSGetCurrentTeb();
}

PPEB OSDLLGetCurrentPeb()
{
	return OSDLLGetCurrentTeb()->Peb;
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
	
	Teb->CurrentDirectory = Peb->StartingDirectory;
	Peb->StartingDirectory = HANDLE_NONE;
	
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
