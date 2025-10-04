#pragma once

#include <pss.h>

typedef struct _TEB
{
	PPEB Peb;
	
	HANDLE CurrentDirectory;
	
	// More to define here such as TLS
}
TEB, *PTEB;

// Get a pointer to the current TEB.
PTEB OSDLLGetCurrentTeb();

// Get a pointer to the current PEB.
PPEB OSDLLGetCurrentPeb();

// Creates a TEB but does not assign it to any thread.
PTEB OSDLLCreateTebObject(PPEB Peb);

// Creates a TEB for the current thread.
BSTATUS OSDLLCreateTeb(PPEB Peb);

// Gets the working directory of the current thread.
HANDLE OSDLLGetCurrentDirectory();
