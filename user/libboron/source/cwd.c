#include <boron.h>
#include "pebteb.h"

HANDLE OSGetCurrentDirectory()
{
	if (OSDLLGetCurrentPeb()->Override.GetCurrentDirectory) {
		return OSDLLGetCurrentPeb()->Override.GetCurrentDirectory();
	}
	
	return OSDLLGetCurrentPeb()->CurrentDirectory;
}

void OSSetCurrentDirectory(HANDLE NewDirectory)
{
	if (OSDLLGetCurrentPeb()->Override.SetCurrentDirectory) {
		return OSDLLGetCurrentPeb()->Override.SetCurrentDirectory(NewDirectory);
	}
	
	PPEB Peb = OSDLLGetCurrentPeb();
	HANDLE OldDirectory = Peb->CurrentDirectory;
	Peb->CurrentDirectory = NewDirectory;
	
	if (OldDirectory)
		OSClose(OldDirectory);
}
