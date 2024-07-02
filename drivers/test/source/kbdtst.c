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

//#define PERFORMANCE_TEST

const char* const KeyboardDeviceName = "\\Devices\\I8042PrtKeyboard"; // <-- this is what the I8042prt driver calls the keyboard

#ifdef PERFORMANCE_TEST
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
#endif

// Table copy pasted from NanoShell.
const unsigned char KeyboardMap[256] =
{
	// shift not pressed.
    0,  27, '1', '2',  '3',  '4', '5', '6',  '7', '8',
  '9', '0', '-', '=', '\x7F', '\t', 'q', 'w',  'e', 'r',
  't', 'y', 'u', 'i',  'o',  'p', '[', ']', '\n',   0,
  'a', 's', 'd', 'f',  'g',  'h', 'j', 'k',  'l', ';',
 '\'', '`',   0,'\\',  'z',  'x', 'c', 'v',  'b', 'n',
  'm', ',', '.', '/',   0,   '*',
    0,	/* Alt */
  ' ',	/* Space bar */
    0,	/* Caps lock */
    0,	/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    0,	/* Up Arrow */
    0,	/* Page Up */
  '-',
    0,	/* Left Arrow */
    0,
    0,	/* Right Arrow */
  '+',
    0,
	0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	
	// shift pressed.
	0,  0, '!', '@', '#', '$', '%', '^', '&', '*',	/* 9 */
  '(', ')', '_', '+', '\x7F',	/* Backspace */
  '\t',			/* Tab */
  'Q', 'W', 'E', 'R',	/* 19 */
  'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\r',	/* Enter key */
    0,			/* 29   - Control */
  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',	/* 39 */
 '"', '~',   0,		/* Left shift */
 '|', 'Z', 'X', 'C', 'V', 'B', 'N',			/* 49 */
  'M', '<', '>', '?',   0,				/* Right shift */
  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
    0,	/* Caps lock */
    0,	/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    0,	/* Up Arrow */
    0,	/* Page Up */
  '-',
    0,	/* Left Arrow */
    0,
    0,	/* Right Arrow */
  '+',
    0,
	0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

#define KEY_LSHIFT 0x2a
#define KEY_RSHIFT 0x36

bool IsShiftPressed = false;

char TranslateKeyCode(char KeyCode)
{
	if (KeyCode == KEY_LSHIFT || KeyCode == KEY_RSHIFT) {
		IsShiftPressed = true;
		return 0;
	}
	
	bool Released = KeyCode & 0x80;
	KeyCode &= ~0x80;
	
	if (KeyCode == KEY_LSHIFT || KeyCode == KEY_RSHIFT) {
		IsShiftPressed = false;
		return 0;
	}
	
	// If the key was released
	if (Released)
		return 0;
	
	return KeyboardMap[IsShiftPressed * 128 + KeyCode];
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
	
#ifdef PERFORMANCE_TEST
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
		
		*Buffer = TranslateKeyCode(*Buffer);
		
		if (*Buffer)
			HalDisplayString(Buffer);
	}
#endif
	
	// Close the device once a key has been pressed
	ObClose(DeviceHandle);
}
