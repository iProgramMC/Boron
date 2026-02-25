#include <boron.h>
#include "testfmk.h"

// This test opens a file, reads some bytes from it, and then closes it.
void Test2BasicRead()
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
	
	IO_STATUS_BLOCK Iosb;
	
	//uint32_t Data;
	//OSReadFile();
	
	
	Status = OSClose(Handle);
	
	if (FAILED(Status))
	{
		TestPrintf("Could not close file: %s", ST(Status));
		return;
	}
}
