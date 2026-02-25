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
	
	TestAssert(SUCCEEDED(Status));
	TestAssert(Handle != HANDLE_NONE);
	
	IO_STATUS_BLOCK Iosb;
	
	uint32_t Data;
	Status = OSReadFile(&Iosb, Handle, 0, &Data, sizeof Data, 0);
	TestAssert(SUCCEEDED(Status));
	TestAssert(SUCCEEDED(Iosb.Status));
	TestAssert(Iosb.BytesRead == sizeof(Data));
	TestAssert(Data == 0x464C457F); // '[127]ELF'
	
	Status = OSClose(Handle);
	TestAssert(SUCCEEDED(Status));
}
