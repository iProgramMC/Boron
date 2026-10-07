#include <boron.h>
#include "testfmk.h"

// This test opens a file, reads some bytes from it, and then closes it.
void Test2ReadExistingFile()
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
	
	// As of 6/10/2026, reading 8 bytes from this offset reads 'libboron'. But don't assume.
	// Just check whether you can read.
#ifdef TARGET_AMD64
	uint64_t Offset = 0x2A8;
#elif defined TARGET_I386
	uint64_t Offset = 0x154;
#else //if defined TARGET_ARM
	uint64_t Offset = 0x1000;
#endif
	
	char Buffer[9];
	Buffer[8] = 0;
	Status = OSReadFile(&Iosb, Handle, Offset, Buffer, sizeof(Buffer) - 1, 0);
	TestAssert(SUCCEEDED(Status));
	TestAssert(SUCCEEDED(Iosb.Status));
	TestAssert(Iosb.BytesRead == sizeof(Buffer) - 1);
	
	if (strcmp(Buffer, "libboron") != 0) {
		TestPrintf("Warning: Buffer is '%s', not 'libboron'. Maybe a sign of failure.", Buffer);
	}
	
	Status = OSClose(Handle);
	TestAssert(SUCCEEDED(Status));
}
