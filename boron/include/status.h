/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	status.h
	
Abstract:
	This header file contains the status code enumeration
	definition for the Boron operating system.
	
Author:
	iProgramInCpp - 30 October 2023
***/
#pragma once

typedef int BSTATUS, *PBSTATUS;

enum
{
	STATUS_SUCCESS,             // Must ALWAYS be defined as zero!
	
	// Generic errors
	STATUS_INVALID_PARAMETER,
	STATUS_ACCESS_DENIED,
	STATUS_INSUFFICIENT_MEMORY,
	STATUS_UNIMPLEMENTED,       // N.B. Ideally you'd only see this when using an incomplete version of the system.
	
	// Wait for object(s) errors
	STATUS_WAITING = 1000,      // these 3 are returned by the KeWaitFor*Object(s) functions.
	STATUS_ALERTED,             // Received a user APC while waiting
	STATUS_TIMEOUT,             // Timeout or would block
	STATUS_KILLED,              // Thread was killed
	STATUS_KERNEL_APC,          // Received a kernel APC, resume waiting
	
	// Probe errors
	STATUS_FAULT = 2000,        // returned by MmProbeAddress when a bad page fault was triggered.
	STATUS_NO_REMAP,            // returned by MmProbeAddress if remapping in kernel space failed, or by MmMapMDL if the MDL is already mapped
	
	// Object errors
	STATUS_NAME_INVALID = 3000, // If the object's name is invalid
	STATUS_NAME_COLLISION,      // If there is already an object with the same name in the directory.
	STATUS_TYPE_MISMATCH,       // The object is not of the expected type
	STATUS_OBJECT_UNOWNED,      // The object is owned by kernel mode but an attempt to access it from user mode was made
	STATUS_NAME_NOT_FOUND,      // If the object name was not found
	STATUS_UNSUPPORTED_FUNCTION,// If the object does not support performing a certain operation
	STATUS_PATH_INVALID,        // If the path is invalid, such as if parse succeeded and matched an object, but path wasn't consumed fully
	STATUS_DIRECTORY_DONE,      // There are no more directory entries to list
	STATUS_LOOP_TOO_DEEP,       // Symbolic link loop was too deep.
	STATUS_UNASSIGNED_LINK,     // A symbolic link was unassigned.
	STATUS_PATH_TOO_DEEP,       // The path is too deep.
	STATUS_NAME_TOO_LONG,       // The path or name is too long.
	STATUS_NOT_LINKED,          // The object is not linked to a directory.
	STATUS_ALREADY_LINKED,      // The object is already linked to a directory.
	STATUS_INVALID_HANDLE,      // The handle is invalid.
	
	// Handle table errors
	STATUS_TABLE_NOT_EMPTY,     // The handle table is not empty.
	STATUS_DELETE_CANCELED,     // The handle delete operation was canceled.
	STATUS_TOO_MANY_HANDLES,    // Too many handles have been opened.
	
	// I/O errors/status codes
	STATUS_PENDING,             // Operation in progress
	STATUS_NEXT_FRAME,
	STATUS_SAME_FRAME,
	STATUS_NO_MORE_FRAMES,
	STATUS_MORE_PROCESSING_REQUIRED, // More processing required for this operation
	STATUS_INSUFFICIENT_SPACE,  // No space left on device
	STATUS_NO_SUCH_DEVICES,     // No devices with specified properties found
	STATUS_UNLOAD,              // Returned by the driver entry.  Tells the driver loader that this driver should be unloaded.
	STATUS_NOT_A_DIRECTORY,     // Not a directory
	
	// Wait for object(s) error ranges
	STATUS_RANGE_WAIT           = 0x1000000, // range 0..MAXIMUM_WAIT_BLOCKS
	STATUS_RANGE_ABANDONED_WAIT = 0x1000040, // range 0..MAXIMUM_WAIT_BLOCKS
	
	// Status codes used internally
#ifdef KERNEL
	STATUS_NO_SEGMENTS_AFTER_THIS = 0x2000000,
#endif
};

#define STATUS_WAIT(n) (STATUS_RANGE_WAIT + (n))
#define STATUS_ABANDONED_WAIT(n) (STATUS_RANGE_ABANDONED_WAIT + (n))

#define SUCCEEDED(x) ((x) == STATUS_SUCCESS)
#define FAILED(x)    ((x) != STATUS_SUCCESS)