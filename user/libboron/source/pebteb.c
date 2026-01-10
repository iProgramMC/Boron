#include <boron.h>
#include <string.h>
#include "pebteb.h"

PTEB OSDLLGetCurrentTeb()
{
	return (PTEB) OSGetCurrentTeb();
}

PPEB OSDLLGetCurrentPeb()
{
	return (PPEB) OSGetCurrentPeb();
}

PTEB OSDLLCreateTebObject(PPEB Peb, HANDLE CurrentDirectory)
{
	if (!Peb)
		Peb = OSDLLGetCurrentPeb();
	
	PTEB Teb = OSAllocate(sizeof(TEB));
	if (!Teb)
		return NULL;
	
	memset(Teb, 0, sizeof *Teb);
	
	// Initialize the TEB structure.
	Teb->Peb = Peb;
	
	Teb->CurrentDirectory = CurrentDirectory;
	
	return Teb;
}

BSTATUS OSDLLCreateTeb(PPEB Peb, HANDLE CurrentDirectory)
{
	PTEB Teb = OSDLLCreateTebObject(Peb, CurrentDirectory);
	if (!Teb)
		return STATUS_INSUFFICIENT_MEMORY;
	
	OSSetCurrentTeb(Teb);
	return STATUS_SUCCESS;
}
