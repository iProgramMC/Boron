#pragma once

#include <handle.h>

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

// Gets the working directory of the current thread.
HANDLE OSGetCurrentDirectory();

// Sets the current working directory of the current thread.
void OSSetCurrentDirectory(HANDLE NewDirectory);
