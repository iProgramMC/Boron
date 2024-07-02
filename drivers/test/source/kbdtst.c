/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	kbdtst.c
	
Abstract:
	This module implements the keyboard test for the test driver.
	
Author:
	iProgramInCpp - 30 June 2024
***/
#include "utils.h"
#include <io.h>
#include <hal.h>

const char* const KeyboardDeviceName = "\\Devices\\I8042PrtKeyboard"; // <-- this is what the I8042prt driver calls the keyboard

void IopsPerformanceTest(HANDLE DeviceHandle)
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
		
		char Buffer[2] = { 0, 0 };
		
		Status = IoReadFile(&Iosb, DeviceHandle, Buffer, 1, true);
		if (FAILED(Status))
			KeCrash("Failed to read, IoReadFile returned %d.  Performed %d iops before fail", Status, iops);
		
		iops++;
	}
}

void PerformKeyboardTest()
{
	BSTATUS Status;
	
	// Wait a half sec just to be sure. Ideally this wouldn't happen
	PerformDelay(500, NULL);
	
	// Open the device
	HANDLE DeviceHandle = HANDLE_NONE;
	
	Status = ObOpenObjectByName(
		KeyboardDeviceName,
		HANDLE_NONE, // RootDirectory
		0,           // Flags
		IoFileType,
		&DeviceHandle
	);
	
	LogMsg("Opened keyboard device, got handle %d, status %d.", DeviceHandle, Status);
	
	if (FAILED(Status))
		KeCrash("Failed to open %s, ObOpenDeviceByName returned %d", KeyboardDeviceName, Status);
	
	HalDisplayString("Type anything: ");
	
#if 1
	IopsPerformanceTest(DeviceHandle);
#else
	IO_STATUS_BLOCK Iosb;
	
	// Read from the device in a continuous fashion
	while (true)
	{
		char Buffer[2] = { 0, 0 };
		
		Status = IoReadFile(&Iosb, DeviceHandle, Buffer, 1, true);
		if (FAILED(Status))
			KeCrash("Failed to read, IoReadFile returned %d.", Status);
		
		HalDisplayString(Buffer);
	}
#endif
	
	// Close the device once a key has been pressed
	ObClose(DeviceHandle);
}
