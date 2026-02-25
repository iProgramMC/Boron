#include <boron.h>
#include "testfmk.h"

// This test opens a file and then closes it.
void Test1BasicOpenAndClose()
{
	OBJECT_ATTRIBUTES Attributes;
	OSInitializeObjectAttributes(&Attributes);
	OSSetNameObjectAttributes(&Attributes, "/bin/TestHarness.exe");
	
	HANDLE Handle;
	BSTATUS Status = OSOpenFile(&Handle, &Attributes);
	
	if (FAILED(Status))
	{
		TestPrintf("Could not open '%s': %s", Attributes.ObjectName, ST(Status));
		return;
	}
	
	Status = OSClose(Handle);
	
	if (FAILED(Status))
	{
		TestPrintf("Could not close file: %s", ST(Status));
		return;
	}
}
