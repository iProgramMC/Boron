#include <boron.h>
#include "testfmk.h"

void Test8MapBigFileAndFlush()
{
	BSTATUS Status;
	HANDLE FileHandle;
	OBJECT_ATTRIBUTES Attributes;

	OSInitializeObjectAttributes(&Attributes);
	OSSetNameObjectAttributes(&Attributes, "/::/Devices/Nvme0Disk1");
	Status = OSOpenFile(&FileHandle, &Attributes);
	TestAssert(SUCCEEDED(Status));
	TestAssert(FileHandle != HANDLE_NONE);

	void* BaseAddress = NULL;
	size_t ViewSize = 16384;
	Status = OSMapViewOfObject(
		CURRENT_PROCESS_HANDLE,
		FileHandle,
		&BaseAddress,
		ViewSize,
		MEM_COMMIT,
		102400, // SectionOffset
		PAGE_READ | PAGE_WRITE
	);
	TestAssert(SUCCEEDED(Status));

	memset(BaseAddress, 'g', ViewSize);

	//
	// Partially flush: only BaseAddress+4096 - BaseAddress+12288
	//
	// The idea is that we flush this range immediately, then open a hex editor and
	// look at the image, then shut down the VM and see that the rest of the changes
	// have been made, too.
	//
	void* FlushAddress = (void*)((uintptr_t) BaseAddress + 4096);
	Status = OSFlushVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		FlushAddress,
		ViewSize / 2,
		false // Async
	);

	Status = OSFreeVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		BaseAddress,
		ViewSize,
		MEM_RELEASE
	);
	TestAssert(SUCCEEDED(Status));

	Status = OSClose(FileHandle);
	TestAssert(SUCCEEDED(Status));
}
