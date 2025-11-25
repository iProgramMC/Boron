// 10 November 2025 - iProgramInCpp
#include <boron.h>
#include <string.h>

#define CHECK_FAILURE() do { \
	if (FAILED(Status)) {    \
		DbgPrint("Failure on line %d: %s (%d)", __LINE__, RtlGetStatusString(Status), Status); \
		OSExitProcess(1);    \
	}                        \
} while (0)

int _start()
{
	// write my own test code here
	void* BaseAddress = NULL;
	size_t RegionSize1 = 1024 * 1024;
	BSTATUS Status = OSAllocateVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		&BaseAddress,
		&RegionSize1,
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READ | PAGE_WRITE
	);
	CHECK_FAILURE();
	DbgPrint("OSAllocateVirtualMemory returned base address %p.", BaseAddress);
	
	// split the range in two
	void* BaseAddress2 = (uint8_t*)BaseAddress + 32 * 1024;
	size_t RegionSize2 = 960 * 1024;
	Status = OSAllocateVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		&BaseAddress2,
		&RegionSize2,
		MEM_COMMIT | MEM_RESERVE | MEM_FIXED | MEM_OVERRIDE,
		PAGE_READ | PAGE_WRITE
	);
	CHECK_FAILURE();
	DbgPrint("Split succeeded. BaseAddress2: %p", BaseAddress2);
	
	// override the newly allocated range on either side
	size_t RegionSize3 = 64 * 1024, RegionSize4 = 64 * 1024;
	void* BaseAddress3 = BaseAddress2, *BaseAddress4 = (uint8_t*)BaseAddress2 + RegionSize2 - RegionSize4;
	Status = OSAllocateVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		&BaseAddress3,
		&RegionSize3,
		MEM_COMMIT | MEM_RESERVE | MEM_FIXED | MEM_OVERRIDE,
		PAGE_READ | PAGE_WRITE
	);
	CHECK_FAILURE();
	DbgPrint("Left side overlap succeeded. BaseAddress3: %p", BaseAddress3);
	
	Status = OSAllocateVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		&BaseAddress4,
		&RegionSize4,
		MEM_COMMIT | MEM_RESERVE | MEM_FIXED | MEM_OVERRIDE,
		PAGE_READ | PAGE_WRITE
	);
	CHECK_FAILURE();
	DbgPrint("Right side overlap succeeded. BaseAddress3: %p", BaseAddress4);
	
	// map the entire center.
	void* BaseAddress5 = (uint8_t*)BaseAddress3 + RegionSize3;
	size_t RegionSize5 = RegionSize2 - RegionSize3 - RegionSize4;
	Status = OSAllocateVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		&BaseAddress5,
		&RegionSize5,
		MEM_COMMIT | MEM_RESERVE | MEM_FIXED | MEM_OVERRIDE,
		PAGE_READ | PAGE_WRITE
	);
	CHECK_FAILURE();
	DbgPrint("Full overlap succeeded. BaseAddress5: %p", BaseAddress5);
	
	// how about remapping the entire region PLUS some free space?
	void* BaseAddress6 = BaseAddress;
	size_t RegionSize6 = RegionSize1 + 512 * 1024;
	Status = OSAllocateVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		&BaseAddress5,
		&RegionSize5,
		MEM_COMMIT | MEM_RESERVE | MEM_FIXED | MEM_OVERRIDE,
		PAGE_READ | PAGE_WRITE
	);
	CHECK_FAILURE();
	DbgPrint("Full overlap + free space succeeded. BaseAddress5: %p", BaseAddress5);
	
	memset(BaseAddress6, 0, RegionSize6);
	DbgPrint("Memset of the entire region succeeded. BaseAddress3: %p", BaseAddress4);
	
	PPEB Peb = OSGetCurrentPeb();
	while (true)
	{
		OSPrintf("Type something in: ");
		
		char Buffer[500];
		IO_STATUS_BLOCK Iosb;
		BSTATUS Status = OSReadFile(&Iosb, Peb->StandardInput, 0, Buffer, sizeof Buffer - 1, IO_RW_FINISH_ON_NEWLINE);
		if (IOFAILED(Status))
		{
			OSPrintf("FAILED to read: %s\n", RtlGetStatusString(Status));
			OSSleep(2000);
		}

		if (FAILED(Status) && !IOFAILED(Status))
			DbgPrint("Note: read finished early: %s", RtlGetStatusString(Status));
		
		Buffer[sizeof Buffer - 1] = 0;
		Buffer[Iosb.BytesRead] = 0;
		OSPrintf("You typed in: '%s'.\n", Buffer);
	}
}
