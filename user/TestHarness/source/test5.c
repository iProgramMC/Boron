#include <boron.h>
#include "testfmk.h"

bool Test5CheckEntireBuffer(void* BufferPtr, uint8_t Byte, size_t BufferSize)
{
	uint8_t* Buffer = BufferPtr;
	
	for (size_t i = 0; i < BufferSize; i++)
	{
		if (Byte != Buffer[i]) {
			DbgPrint("Test5CheckEntireBuffer: diff found at %zu. Is %02x but needs to be %02x.", i, Buffer[i], Byte);
			return false;
		}
	}
	
	return true;
}

void Test5ForkCurrentProcess()
{
	BSTATUS Status;
	HANDLE ChildHandle;
	void *SharedBuffer, *NonSharedBuffer;
	size_t SharedBufferSize = 4096, NonSharedBufferSize = 4096;
	
	OSPrintf("Allocating a shared and non-shared buffer.\n");
	
	Status = OSAllocateVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		&SharedBuffer,
		&SharedBufferSize,
		MEM_RESERVE | MEM_COMMIT | MEM_SHARED,
		PAGE_READ | PAGE_WRITE
	);
	TestAssert(SUCCEEDED(Status));
	
	Status = OSAllocateVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		&NonSharedBuffer,
		&NonSharedBufferSize,
		MEM_RESERVE | MEM_COMMIT,
		PAGE_READ | PAGE_WRITE
	);
	TestAssert(SUCCEEDED(Status));
	
	OSPrintf("Clearing shared buffer and non-shared buffer to unique data.\n");
	
	uint8_t SharedBufferParentsMarker = 0xAA;
	uint8_t NonSharedBufferParentsMarker = 0xBB;
	uint8_t SharedBufferChildsMarker = 0xCC;
	uint8_t NonSharedBufferChildsMarker = 0xDD;
	
	memset(SharedBuffer, SharedBufferParentsMarker, SharedBufferSize);
	memset(NonSharedBuffer, NonSharedBufferParentsMarker, NonSharedBufferSize);
	
	OSPrintf("About to fork...\n");
	
	Status = OSForkProcess(&ChildHandle);
	if (Status == STATUS_IS_CHILD_PROCESS)
	{
		// We are the child process.
		OSPrintf("Hello from the child process...\n");
		
		// Reset the shared buffer and non-shared buffer to our own data.
		// The parent should be able to see the SharedBufferChildsMarker,
		// but not the NonSharedBufferChildsMarker.
		memset(SharedBuffer, SharedBufferChildsMarker, SharedBufferSize);
		memset(NonSharedBuffer, NonSharedBufferChildsMarker, NonSharedBufferSize);
		
		OSExitProcess(0);
	}
	
	TestAssert(SUCCEEDED(Status));
	
	OSPrintf("Waiting for the child process to exit...\n");
	Status = OSWaitForSingleObject(ChildHandle, false, WAIT_TIMEOUT_INFINITE);
	TestAssert(SUCCEEDED(Status));
	
	// Finally, ensure that the shared buffer was modified, but the non-shared
	// buffer wasn't.
	TestAssert(Test5CheckEntireBuffer(SharedBuffer, SharedBufferChildsMarker, SharedBufferSize));
	TestAssert(Test5CheckEntireBuffer(NonSharedBuffer, NonSharedBufferParentsMarker, NonSharedBufferSize));
	
	OSPrintf("This test was a success.\n");
}
