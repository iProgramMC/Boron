#include <boron.h>
#include "pebteb.h"

HANDLE OSGetCurrentDirectory()
{
	if (OSDLLGetCurrentPeb()->Override.GetCurrentDirectory) {
		return OSDLLGetCurrentPeb()->Override.GetCurrentDirectory();
	}
	
	DbgPrint("OSDLL: Calling OSGetCurrentDirectory()");
	return OSDLLGetCurrentTeb()->CurrentDirectory;
}

void OSSetCurrentDirectory(HANDLE NewDirectory)
{
	if (OSDLLGetCurrentPeb()->Override.GetCurrentDirectory) {
		return OSDLLGetCurrentPeb()->Override.SetCurrentDirectory(NewDirectory);
	}
	
	PTEB Teb = OSDLLGetCurrentTeb();
	HANDLE OldDirectory = Teb->CurrentDirectory;
	Teb->CurrentDirectory = NewDirectory;
	
	if (OldDirectory)
		OSClose(OldDirectory);
}
