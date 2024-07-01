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

const char* const KeyboardDeviceName = "\\Devices\\I8042PrtKeyboard"; // <-- this is what the I8042prt driver calls the keyboard

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
	
	// Read from the device in a continuous fashion
	
	// Close the device once a key has been pressed
	ObClose(DeviceHandle);
}
