/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	pipetst.c
	
Abstract:
	This module implements the pipe test for the test driver.
	
Author:
	iProgramInCpp - 15 September 2025
***/
#include <io.h>
#include <ob.h>
#include <ex.h>
#include <string.h>

uint8_t SomeData[4096];

void PerformPipeTest()
{
	HANDLE Handle;
	BSTATUS Status;
	IO_STATUS_BLOCK Iosb;
	
	// memcpy from the start of the HHDM to get some data going
	memcpy(SomeData, (void*) MmGetHHDMOffsetAddr(0), sizeof SomeData);
	
	const size_t BufferSize = 4096;
	
	Status = OSCreatePipe(&Handle, NULL, BufferSize);
	if (FAILED(Status))
		KeCrash("Pipe: Failed to OSCreatePipe: %d (%s)", Status, RtlGetStatusString(Status));
	
	LogMsg("Pipe: Trying to write %zu bytes to the pipe...", BufferSize - 1);
	Status = OSWriteFile(&Iosb, Handle, 0, SomeData, BufferSize - 1, 0, NULL);
	if (FAILED(Status))
		KeCrash("Pipe: Failed to OSWriteFile into the pipe (1): %d (%s)", Status, RtlGetStatusString(Status));
	
	// Test writing over the buffer size.
	LogMsg("Pipe: Trying to write another byte.");
	Status = OSWriteFile(&Iosb, Handle, 0, SomeData, 1, 0, NULL);
	if (IOFAILED(Status))
		KeCrash("Pipe: Failed to OSWriteFile into the pipe (2): %d (%s)", Status, RtlGetStatusString(Status));
	
	// Since the buffer is full, and we are the only reader and writer,
	// this should return STATUS_END_OF_FILE.
	if (Status != STATUS_END_OF_FILE)
	{
		KeCrash(
			"Pipe: Write finished with status %d (%s). Expected %d (%s)",
			Status, RtlGetStatusString(Status),
			STATUS_END_OF_FILE, RtlGetStatusString(STATUS_END_OF_FILE)
		);
	}
	
	// Now trying to read the same data
	LogMsg("Pipe: Trying to read %zu bytes from the pipe...", BufferSize - 1);
	uint8_t NewBuffer[4096];
	Status = OSReadFile(&Iosb, Handle, 0, NewBuffer, BufferSize - 1, 0);
	if (FAILED(Status))
		KeCrash("Pipe: Failed to OSReadFile from the pipe (1): %d (%s)", Status, RtlGetStatusString(Status));
	
	if (memcmp(NewBuffer, SomeData, BufferSize - 1) != 0)
		KeCrash("Pipe: Comparison between NewBuffer and SomeData failed!");
	else
		LogMsg("Pipe: Comparison succeeded between NewBuffer and SomeData.");
	
	// Try over-reading
	LogMsg("Pipe: Trying to read another byte.");
	Status = OSReadFile(&Iosb, Handle, 0, NewBuffer, 1, 0);
	if (IOFAILED(Status))
		KeCrash("Pipe: Failed to OSReadFile from the pipe (2): %d (%s)", Status, RtlGetStatusString(Status));
	
	// Since the buffer is empty, and we are the only reader and writer,
	// this should return STATUS_END_OF_FILE.
	if (Status != STATUS_END_OF_FILE)
	{
		KeCrash(
			"Pipe: Read finished with status %d (%s). Expected %d (%s)",
			Status, RtlGetStatusString(Status),
			STATUS_END_OF_FILE, RtlGetStatusString(STATUS_END_OF_FILE)
		);
	}
	
	LogMsg("Pipe: Test finished. Closing handle.");
	Status = OSClose(Handle);
	if (FAILED(Status))
		KeCrash("Pipe: Failed to close handle: %d (%s)", Status, RtlGetStatusString(Status));
}

