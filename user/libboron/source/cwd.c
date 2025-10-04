#include <boron.h>
#include "pebteb.h"

HANDLE OSDLLGetCurrentDirectory()
{
	return OSDLLGetCurrentTeb()->CurrentDirectory;
}
