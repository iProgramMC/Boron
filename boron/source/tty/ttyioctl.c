/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ttyioctl.c
	
Abstract:
	This module implements the TTY (Virtual Teletype)
	device I/O control routine.
	
Author:
	iProgramInCpp - 23 November 2025
***/
#include "ttyi.h"
#include <string.h>

static BSTATUS TtyDeviceIoControlIn(PTERMINAL Terminal, int IoControlCode, const void* InBuffer, size_t InBufferSize, void* OutBuffer, size_t OutBufferSize)
{
	switch (IoControlCode)
	{
		case IOCTL_TERMINAL_GET_TERMINAL_STATE:
		{
			// No data taken in.
			if (InBufferSize != 0)
				return STATUS_INVALID_PARAMETER;
			
			// This many bytes put out.
			if (OutBufferSize != sizeof(TERMINAL_STATE))
				return STATUS_INVALID_PARAMETER;
			
			memcpy(OutBuffer, &Terminal->State, sizeof(TERMINAL_STATE));
			return STATUS_SUCCESS;
		}
		case IOCTL_TERMINAL_SET_TERMINAL_STATE:
		{
			// This many bytes taken in.
			if (InBufferSize != sizeof(TERMINAL_STATE))
				return STATUS_INVALID_PARAMETER;
			
			// No bytes put out.
			if (OutBufferSize != 0)
				return STATUS_INVALID_PARAMETER;
			
			memcpy(&Terminal->State, InBuffer, sizeof(TERMINAL_STATE));
			return STATUS_SUCCESS;
		}
		case IOCTL_TERMINAL_GET_WINDOW_SIZE:
		{
			// No data taken in.
			if (InBufferSize != 0)
				return STATUS_INVALID_PARAMETER;
			
			// This many bytes put out.
			if (OutBufferSize != sizeof(TERMINAL_WINDOW_SIZE))
				return STATUS_INVALID_PARAMETER;
			
			memcpy(OutBuffer, &Terminal->WindowSize, sizeof(TERMINAL_WINDOW_SIZE));
			return STATUS_SUCCESS;
		}
		case IOCTL_TERMINAL_SET_WINDOW_SIZE:
		{
			// This many bytes taken in.
			if (InBufferSize != sizeof(TERMINAL_WINDOW_SIZE))
				return STATUS_INVALID_PARAMETER;
			
			// No bytes put out.
			if (OutBufferSize != 0)
				return STATUS_INVALID_PARAMETER;
			
			memcpy(&Terminal->WindowSize, InBuffer, sizeof(TERMINAL_WINDOW_SIZE));
			return STATUS_SUCCESS;
		}
	}
	
	return STATUS_UNSUPPORTED_FUNCTION;
}

BSTATUS TtyDeviceIoControl(PTERMINAL Terminal, int IoControlCode, const void* InBuffer, size_t InBufferSize, void* OutBuffer, size_t OutBufferSize)
{
	BSTATUS Status = KeWaitForSingleObject(&Terminal->StateMutex, true, TIMEOUT_INFINITE, KeGetPreviousMode());
	if (FAILED(Status))
		return Status;
	
	Status = TtyDeviceIoControlIn(Terminal, IoControlCode, InBuffer, InBufferSize, OutBuffer, OutBufferSize);
	
	KeReleaseMutex(&Terminal->StateMutex);
	return Status;
}