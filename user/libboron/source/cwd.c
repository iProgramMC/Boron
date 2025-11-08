#include <boron.h>
#include "pebteb.h"

HANDLE OSGetCurrentDirectory()
{
	return OSDLLGetCurrentTeb()->CurrentDirectory;
}

void OSSetCurrentDirectory(HANDLE NewDirectory)
{
	PTEB Teb = OSDLLGetCurrentTeb();
	HANDLE OldDirectory = Teb->CurrentDirectory;
	Teb->CurrentDirectory = NewDirectory;
	
	if (OldDirectory)
		OSClose(OldDirectory);
}
