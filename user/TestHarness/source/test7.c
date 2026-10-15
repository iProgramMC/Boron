#include <boron.h>
#include "testfmk.h"

//#define CREATE_THE_FILE

void Test7CreateBigFileAndFlush()
{
#if CREATE_THE_FILE
	const char* Directory = "/";
	const char* FileName = "LargeTestFile";
#else
	const char* FileName = "/::/Devices/Nvme0Disk1Part0";
#endif

	BSTATUS Status;
	HANDLE Handle, RootDirHandle = HANDLE_NONE;
	OBJECT_ATTRIBUTES Attributes;

#if CREATE_THE_FILE

	OSInitializeObjectAttributes(&Attributes);
	OSSetNameObjectAttributes(&Attributes, Directory);

	// open the root directory to create a file there.
	Status = OSOpenFile(&RootDirHandle, &Attributes);

	TestAssert(SUCCEEDED(Status));
	TestAssert(RootDirHandle != HANDLE_NONE);

	// try creating a file in there.
	Status = OSCreateFile(&Handle, RootDirHandle, FileName, strlen(FileName));
	TestAssert(SUCCEEDED(Status));
	TestAssert(Handle != HANDLE_NONE);

#else

	OSInitializeObjectAttributes(&Attributes);
	OSSetNameObjectAttributes(&Attributes, FileName);
	Status = OSOpenFile(&Handle, &Attributes);
	TestAssert(SUCCEEDED(Status));
	TestAssert(Handle != HANDLE_NONE);

#endif

	const char* DataToWrite = "Boron is cool!\n";
	const size_t Iters = 1000000;
	for (size_t i = 0; i < Iters; i++)
	{
		IO_STATUS_BLOCK Iosb;
		uint64_t OutSize = 0;
		DbgPrint("Test7: Writing %zu/%zu", i+1, Iters);
		Status = OSWriteFile(
			&Iosb,
			Handle,
			0, // ByteOffset
			DataToWrite,
			strlen(DataToWrite),
			IO_RW_SHARED_FILE_OFFSET,
			&OutSize
		);

		TestAssert(SUCCEEDED(Status));
		TestAssert(SUCCEEDED(Iosb.Status));
		TestAssert(Iosb.BytesWritten == strlen(DataToWrite));
	}

	// NOTE: For tmpfs, this won't do anything, but I need a case to debug it with.
	Status = OSFlushFileCache(Handle);
	if (FAILED(Status)) {
		DbgPrint("Status: %s", RtlGetStatusString(Status));
	}
	TestAssert(SUCCEEDED(Status));

	Status = OSClose(Handle);
	TestAssert(SUCCEEDED(Status));

	if (RootDirHandle != HANDLE_NONE) {
		Status = OSClose(RootDirHandle);
		TestAssert(SUCCEEDED(Status));
	}
}
