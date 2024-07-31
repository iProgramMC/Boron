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

const char* const StorDeviceName = "\\Devices\\Nvme0Disk1"; // <-- this is what the NVMe driver calls the first nvme device

void DumpHex(void* DataV, size_t DataSize, bool LogScreen)
{
	uint8_t* Data = DataV;
	
	#define A(x) (((x) >= 0x20 && (x) <= 0x7F) ? (x) : '.')
	
	const size_t PrintPerRow = 32;
	
	for (size_t i = 0; i < DataSize; i += PrintPerRow) {
		char Buffer[256];
		Buffer[0] = 0;
		
		sprintf(Buffer + strlen(Buffer), "%04lx: ", i);
		
		for (size_t j = 0; j < PrintPerRow; j++) {
			if (i + j >= DataSize)
				strcat(Buffer, "   ");
			else
				sprintf(Buffer + strlen(Buffer), "%02x ", Data[i + j]);
		}
		
		strcat(Buffer, "  ");
		
		for (size_t j = 0; j < PrintPerRow; j++) {
			if (i + j >= DataSize)
				strcat(Buffer, " ");
			else
				sprintf(Buffer + strlen(Buffer), "%c", A(Data[i + j]));
		}
		
		if (LogScreen)
			LogMsg("%s", Buffer);
		else
			DbgPrint("%s", Buffer);
	}
}

#ifdef PERFORMANCE_TEST
void IopsPerformanceTest(HANDLE DeviceHandle, void* Buffer, size_t BufferSize)
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
		
		Status = IoReadFile(&Iosb, DeviceHandle, Buffer, BufferSize, 0);
		if (FAILED(Status))
			KeCrash("Failed to read, IoReadFile returned %d.  Performed %d iops before fail", Status, iops);
		
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
	Status = IoGetAlignmentInfo(DeviceHandle, &Alignment);
	ASSERT(SUCCEEDED(Status));
	
	Alignment *= 2;
	void* Buffer = MmAllocatePool(POOL_NONPAGED, Alignment);
	
	if (FAILED(Status))
		KeCrash("Failed to open %s, ObOpenDeviceByName returned %d", StorDeviceName, Status);
	
	LogMsg("Reading...");
	
#ifdef PERFORMANCE_TEST
	IopsPerformanceTest(DeviceHandle, Buffer, Alignment);
#else
	Status = IoReadFile(&Iosb, DeviceHandle, Buffer, Alignment, 0);
	if (FAILED(Status))
		KeCrash("Failed to read, IoReadFile returned %d.", Status);
	
	DumpHex(Buffer, MIN(1024, Alignment), true);
#endif
	
	// Close the device once a key has been pressed
	ObClose(DeviceHandle);
}
