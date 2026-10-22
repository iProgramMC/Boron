#include <boron.h>
#include "testfmk.h"

// This test opens a file, reads some bytes from it, and then closes it.
void Test3CreateFile()
{
	HANDLE Handle;
	BSTATUS Status;

	OBJECT_ATTRIBUTES Attributes;
	OSInitializeObjectAttributes(&Attributes);
	OSSetNameObjectAttributes(&Attributes, "/etc/test_file.txt");

	// Try creating a file in the specified path.  As of 22/10/2026, this points
	// to a tmpfs directory.
	Status = OSCreateFile(
		&Handle,
		&Attributes,
		NULL,   // Iosb
		0,      // FileAttributes
		0,      // AllocationSize
		IO_SUPERSEDE,          // CreateDisposition
		IO_FILE_REGULAR_FILE,  // CreateOptions
		NULL,   // TargetName
		0       // TargetNameLength
	);

	if (FAILED(Status)) {
		DbgPrint("ERROR: %s", RtlGetStatusString(Status));
	}

	TestAssert(SUCCEEDED(Status));
	TestAssert(Handle != HANDLE_NONE);

	// try writing some data
	const char* DataToWrite = "Boron is cool!\n";
	IO_STATUS_BLOCK Iosb;
	uint64_t OutSize = 0;
	Status = OSWriteFile(&Iosb, Handle, 0, DataToWrite, strlen(DataToWrite), 0, &OutSize);

	TestAssert(SUCCEEDED(Status));
	TestAssert(SUCCEEDED(Iosb.Status));
	TestAssert(Iosb.BytesWritten == strlen(DataToWrite));
	TestAssert(OutSize == strlen(DataToWrite));

	// then try reading it back (and more, to also test EOF detection)
	char Buffer[24];
	Status = OSReadFile(&Iosb, Handle, 0, Buffer, sizeof(Buffer), 0);

	TestAssert(IOSUCCEEDED(Status));
	TestAssert(IOSUCCEEDED(Iosb.Status));
	TestAssert(Iosb.BytesRead == strlen(DataToWrite));
	TestAssert(memcmp(DataToWrite, Buffer, strlen(DataToWrite)) == 0);

	Status = OSClose(Handle);
	TestAssert(SUCCEEDED(Status));
}
