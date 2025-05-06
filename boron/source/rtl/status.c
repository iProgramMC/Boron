/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	rtl/status.c
	
Abstract:
	This module contains string descriptions for each status code.
	
Author:
	iProgramInCpp - 19 April 2025
***/
#include "status.h"

// TODO: Sync with ke/thread.h
#define MAXIMUM_WAIT_BLOCKS 64

// TODO: Implement these in a more advanced way, like, show the faulting instruction and stuff like that.
static const char* const RtlpStatusCodes[] =
{
	// NOTE: Keep synced with status.h!
	"Success",
	
	"An invalid parameter was passed to a function or system service.",
	"An instruction referenced memory which could not be accessed.",
	"Not enough memory is available to complete the operation.",
	"This function or system service is unimplemented.",
	"The interrupt priority level that this function was called at is too high.",
	"An instruction has triggered a page fault and the page fault handler has returned without completely resolving the page fault.",
	"An instruction has triggered a page fault and the page fault handler is waiting for memory.",
	
	"The current thread is waiting.",
	"The current thread has been alerted.",
	"The given timeout interval has expired.",
	"The current thread has been killed.",
	"A kernel APC was received, resuming the wait process.",
	
	"An access violation has occurred while trying to access a parameter during a system service.",
	"This object cannot be remapped.",
	
	"The object name is invalid.",
	"The object name already exists.",
	"The object is not of the requested type.",
	"This object is not accessible from user mode.",
	"The object name was not found.",
	"This object does not support this function.",
	"The path is invalid.",
	"There are no more entries in the directory.",
	"Too many symbolic links have been encountered.",
	"The symbolic link does not have an object assigned.",
	"The path is too deep.",
	"The path or name is too long.",
	"The object is not linked to a directory.",
	"The object is already linked to a directory.",
	"The handle is invalid.",
	
	"The handle table is not empty.",
	"The handle delete operation was cancelled.",
	"Too many handles have been opened.",
	
	"The operation is in progress.",
	"The header is invalid.",
	"STATUS_SAME_FRAME",
	"STATUS_NO_MORE_FRAMES",
	"More processing is required for this operation.",
	"No space left on the device.",
	"No devices with the specified properties were found.",
	"The driver is to be unloaded.",
	"The object is not a directory.",
	"An I/O error was reported by the hardware.",
	"The requested operation requires a parameter to be aligned.",
	"This file system does not exist on this volume.",
	
	"Not enough virtual address space is available to complete the operation.",
	"The virtual address requested is not located at the base of a reserved region.",
	"The virtual address requested does not have a reserved memory range associated with it.",
	"The memory could not be released because it hasn't been decommitted in its entirety.",
	"The given address range conflicts with existing address space, or overlaps multiple regions.",
};

const char* RtlGetStatusString(int code)
{
	if (code < 0)
		return "Unknown Error";
	
	if (code >= STATUS_RANGE_ABANDONED_WAIT + MAXIMUM_WAIT_BLOCKS)
		return "Unknown Error";
	
	if (code >= STATUS_RANGE_ABANDONED_WAIT)
		return "The caller attempted to wait for an object that has been abandoned.";
	
	if (code >= STATUS_RANGE_WAIT)
		return "The caller specified WAIT_ANY for the wait type and one of the objects has been set to the signalled state.";
	
	return RtlpStatusCodes[code];
}
