#include <boron.h>

void OSExitThread();
void OSDummy();

void DLLEntryPoint()
{
	OSDummy();
	OSDummy();
	
	OSExitThread();
}
