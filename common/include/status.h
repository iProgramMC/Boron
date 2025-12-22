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
	// NOTE: Keep synced with rtl/status.c!
	STATUS_SUCCESS,             // Must ALWAYS be defined as zero!
	
	// Generic errors
	STATUS_INVALID_PARAMETER,
	STATUS_ACCESS_VIOLATION,
	STATUS_INSUFFICIENT_MEMORY,
	STATUS_UNIMPLEMENTED,       // N.B. Ideally you'd only see this when using an incomplete version of the system.
	STATUS_IPL_TOO_HIGH,        // The operation was attempted at high IPL.
	STATUS_REFAULT,             // The page fault should be re-attempted.
	STATUS_REFAULT_SLEEP,       // The page fault should be re-attempted and the thread should sleep for a bit.
	
	// Wait for object(s) errors
	STATUS_WAITING,             // these 3 are returned by the KeWaitFor*Object(s) functions.
	STATUS_ALERTED,             // Received a user APC while waiting
	STATUS_TIMEOUT,             // Timeout or would block
	STATUS_KILLED,              // Thread was killed
	STATUS_KERNEL_APC,          // Received a kernel APC, resume waiting
	
	// Probe errors
	STATUS_FAULT,               // returned by MmProbeAddress when an invalid page fault was triggered.
	STATUS_NO_REMAP,            // returned by MmProbeAddress if remapping in kernel space failed, or by MmMapMDL if the MDL is already mapped
	
	// Object errors
	STATUS_NAME_INVALID,        // If the object's name is invalid
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
	STATUS_INVALID_HEADER,      // The header is invalid.  First used for MBR/GPT validation but could have more uses
	STATUS_SAME_FRAME,
	STATUS_NO_MORE_FRAMES,
	STATUS_MORE_PROCESSING_REQUIRED, // More processing required for this operation
	STATUS_INSUFFICIENT_SPACE,  // No space left on device
	STATUS_NO_SUCH_DEVICES,     // No devices with specified properties found
	STATUS_UNLOAD,              // Returned by the driver entry.  Tells the driver loader that this driver should be unloaded.
	STATUS_NOT_A_DIRECTORY,     // Not a directory
	STATUS_IS_A_DIRECTORY,      // Target is a directory
	STATUS_HARDWARE_IO_ERROR,   // I/O error reported by backing hardware
	STATUS_UNALIGNED_OPERATION, // Unaligned operation was attempted.  Use IoGetOperationAlignment to find the alignment required to write to this device.
	STATUS_NOT_THIS_FILE_SYSTEM,// This file system does not exist on this volume
	STATUS_END_OF_FILE,         // The end of this file has been reached.
	STATUS_BLOCKING_OPERATION,  // The operation would block the running thread, and the resource is marked non-blocking.
	STATUS_DIRECTORY_NOT_EMPTY, // The directory is not empty.
	STATUS_OUT_OF_FILE_BOUNDS,  // The specified offset is located outside of this file's boundaries.
	STATUS_NOT_A_TERMINAL,      // The specified file is not a terminal
	
	// Memory manager error codes
	STATUS_INSUFFICIENT_VA_SPACE, // Insufficient virtual address space
	STATUS_VA_NOT_AT_BASE,      // The virtual address given is not the beginning of a reserved region
	STATUS_MEMORY_NOT_RESERVED, // There is no memory reserved at that address
	STATUS_MEMORY_COMMITTED,    // There is committed memory but decommit wasn't explicitly requested.
	STATUS_CONFLICTING_ADDRESSES, // The given address range conflicts with existing address space, or overlaps multiple reserved regions, or overlaps unreserved regions.
	
	// Execution error codes
	STATUS_INVALID_EXECUTABLE,  // The executable file is invalid.
	STATUS_INVALID_ARCHITECTURE,// The executable file is valid, but for a machine type other than the current machine.
	STATUS_IS_CHILD_PROCESS,    // After an OSForkProcess() operation is performed, the child process sees this as the return value and the parent sees STATUS_SUCCESS.
	
	// Process error codes.
	STATUS_STILL_RUNNING,       // The process is still running
	
	STATUS_MAX,
	
	// Wait for object(s) error ranges
	STATUS_RANGE_WAIT           = 0x1000000, // range 0..MAXIMUM_WAIT_BLOCKS
	STATUS_RANGE_ABANDONED_WAIT = 0x1000040, // range 0..MAXIMUM_WAIT_BLOCKS
};

#define STATUS_WAIT(n) (STATUS_RANGE_WAIT + (n))
#define STATUS_ABANDONED_WAIT(n) (STATUS_RANGE_ABANDONED_WAIT + (n))

// Use FAILED(Status) and SUCCEEDED(Status) when checking for statuses
// from system services unrelated to I/O.
#define FAILED(x)    ((x) != STATUS_SUCCESS)
#define SUCCEEDED(x) (!FAILED(x))

// Use IOFAILED(Status) and IOSUCCEEDED(Status) when checking for statuses
// from system services related to I/O, as they declare three status codes
// successful instead of just STATUS_SUCCESS.
#define IOFAILED(x)    ((x) != STATUS_SUCCESS && (x) != STATUS_END_OF_FILE && (x) != STATUS_BLOCKING_OPERATION)
#define IOSUCCEEDED(x) (!IOFAILED(x))

#ifdef __cplusplus
extern "C" {
#endif

const char* RtlGetStatusString(int code);

#ifdef __cplusplus
}
#endif
