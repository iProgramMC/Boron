/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	stortst.c
	
Abstract:
	This module implements the storage test for the test driver.
	
Author:
	iProgramInCpp - 30 July 2024
***/
#include "utils.h"
#include <io.h>
#include <hal.h>
#include <string.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

//#define PERFORMANCE_TEST

const char* const StorDeviceName = "/Devices/Nvme0Disk1"; // <-- this is what the NVMe driver calls the first nvme device

#ifdef PERFORMANCE_TEST
static void IopsPerformanceTest(HANDLE DeviceHandle, void* Buffer, size_t BufferSize)
{
	// Read from the device in a continuous fashion
	BSTATUS Status;
	IO_STATUS_BLOCK Iosb;
	uint64_t lastSec = HalGetTickCount();
	uint64_t frequency = HalGetTickFrequency();
	int iops = 0;
	
	while (true)
	{
		if (lastSec + frequency < HalGetTickCount())
		{
			lastSec += frequency;
			LogMsg("Iops: %d", iops);
			iops = 0;
		}
		
		Status = OSReadFile(&Iosb, DeviceHandle, 0, Buffer, BufferSize, 0);
		if (FAILED(Status))
			KeCrash("Failed to read, OSReadFile returned %d.  Performed %d iops before fail", Status, iops);
		
		iops++;
	}
}
#endif

void PerformStorageTest()
{
	BSTATUS Status;
	
	// Open the device
	HANDLE DeviceHandle = HANDLE_NONE;
	
	Status = ObOpenObjectByName(
		StorDeviceName,
		HANDLE_NONE, // RootDirectory
		0,           // Flags
		IoFileType,
		&DeviceHandle
	);
	
	LogMsg("Opened storage device, got handle %d, status %d.", DeviceHandle, Status);
	
	// Fetch alignment info.
	IO_STATUS_BLOCK Iosb = {};
	size_t Alignment;
	Status = OSGetAlignmentFile(DeviceHandle, &Alignment);
	ASSERT(SUCCEEDED(Status));
	
	Alignment = 1024;
	void* Buffer = MmAllocatePool(POOL_NONPAGED, Alignment);
	
	if (FAILED(Status))
		KeCrash("Failed to open %s, ObOpenDeviceByName returned %d", StorDeviceName, Status);
	
	LogMsg("Reading... %p", Buffer);
	memset(Buffer, 0xCC, Alignment);
	
#ifdef PERFORMANCE_TEST
	IopsPerformanceTest(DeviceHandle, Buffer, Alignment);
#else
	Status = OSReadFile(&Iosb, DeviceHandle, 0, Buffer, Alignment, 0);
	if (FAILED(Status))
		KeCrash("Failed to read, OSReadFile returned %d.", Status);
	
	DumpHex(Buffer, Alignment > 1024 ? 1024 : Alignment, true);
	//DumpHex(Buffer, Alignment, false);
#endif
	
	// Close the device once a key has been pressed
	ObClose(DeviceHandle);
}
