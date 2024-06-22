/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/irp.h
	
Abstract:
	This header file defines the I/O Request Packet structure
	and manipulation functions.
	
Author:
	iProgramInCpp - 16 February 2024
***/
#pragma once

#include <ke.h>
#include <mm.h>

// A certain set of IRPs may be sent to a certain device.
//
// The comments for each IRP function will have a sequence
// of letters that tells which device driver type should accept
// what IRP functions.
//
// Key:
//  D - Disk driver
//  F - File system driver
//  K - Keyboard driver
//  M - Mouse driver
//  N - Network driver
//  S - Sound driver
//  T - Terminal driver
//  V - Video driver
//  -- Expand if needed --

enum
{
	IRP_FUN_CLOSE,                     //  0 - DFKMV
	IRP_FUN_CREATE,                    //  1 - DFKMV
	IRP_FUN_READ,                      //  2 - DFKMV
	IRP_FUN_WRITE,                     //  3 - DFKMV
	IRP_FUN_QUERY_INFORMATION,         //  4 - FKMV
	IRP_FUN_SET_INFORMATION,           //  5 - FKMV
	IRP_FUN_DEVICE_CONTROL,            //  6 - DKM
	IRP_FUN_DIRECTORY_CONTROL,         //  7 - F
	IRP_FUN_FILE_SYSTEM_CONTROL,       //  8 - F
	IRP_FUN_LOCK_CONTROL,              //  9 - F
	IRP_FUN_SET_NEW_SIZE,              // 10 - F
	// TODO
	IRP_FUN_R11,
	IRP_FUN_R12,
	IRP_FUN_R13,
	IRP_FUN_R14,
	IRP_FUN_R15,
	IRP_FUN_R16,
	IRP_FUN_R17,
	IRP_FUN_R18,
	IRP_FUN_R19,
	IRP_FUN_R20,
	IRP_FUN_R21,
	IRP_FUN_R22,
	IRP_FUN_R23,
	IRP_FUN_R24,
	IRP_FUN_R25,
	IRP_FUN_R26,
	IRP_FUN_R27,
	IRP_FUN_R28,
	IRP_FUN_R29,
	IRP_FUN_R30,
	IRP_FUN_R31,
	IRP_FUN_COUNT
};

typedef struct _IO_STATUS_BLOCK
{
	BSTATUS Status;
	uintptr_t Information;
}
IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

#define IRP_FLAG_ASYNCHRONOUS     (1 << 0) // IRP is dispatched asynchronously
#define IRP_FLAG_ZONE_ORIGINATED  (1 << 1) // IRP came from a pre-allocated zone
#define IRP_FLAG_FREE_ON_COMPLETE (1 << 2) // IRP should be freed when completed
#define IRP_FLAG_FREE_MDL         (1 << 3) // Unpin and free the MDL when the IRP is completed
#define IRP_FLAG_COMPLETED        (1 << 4) // IRP was completed
#define IRP_FLAG_CANCELLED        (1 << 5) // IRP was cancelled
#define IRP_FLAG_PENDING          (1 << 6) // IRP is currently pending completion

typedef BSTATUS(*IO_COMPLETION_ROUTINE)(PDEVICE_OBJECT Device, PIRP Irp, void* Context);

typedef struct _IO_STACK_FRAME
{
	IO_COMPLETION_ROUTINE CompletionRoutine;
	
	// The code of the function executed by this IRP stack frame.
	uint8_t Function;
	
	// The location of this stack frame within the parent IRP.
	uint8_t StackIndex;
	
	uint32_t Flags;
	
	// Driver-specific. Passed into the CompletionRoutine, if it exists.
	void* Context;
	
	// The file control block affected by this IRP.
	PFCB FileControlBlock;
	
	// The offset and length of the I/O transfer.
	uint64_t Offset;
	uint64_t Length;
	
	// The offset from the virtual base of the MDL where the transfer should be performed.
	uintptr_t MdlOffset;
}
IO_STACK_FRAME, *PIO_STACK_FRAME;

typedef struct _IRP
{
	uint32_t Flags;
	
	KPROCESSOR_MODE ModeRequestor;
	
	int AssociatedIrpCount;
	PIRP AssociatedIrp;
	PIRP ParentIrp;
	
	IO_STATUS_BLOCK StatusBlock;
	
	PKEVENT CompletionEvent;
	
	PKAPC CompletionApc;
	
	// Buffer associated with the IRP.  This is the source/destination buffer
	// that the IRP uses to transfer data.
	// If IRP_FLAG_FREE_MDL is specified, this MDL is unmapped, unpinned, and
	// freed when this IRP is completed.
	PMDL Mdl;
	
	uint8_t CurrentFrameIndex;
	uint8_t StackSize;
	
	// If ModeRequestor == MODE_USER, then this points to a file object that
	// was referenced when this IRP was created. When the IRP is freed, this
	// pointer is dereferenced.
	void* FileObject;
	
	// Used in synchronous mode.  Determines how long to wait for the IRP to
	// finish.
	int Timeout;
	
	// This IRP was allocated with an appendage whose size in bytes is
	// sizeof(IO_STACK_FRAME) * StackSize.
	IO_STACK_FRAME Stack[];
}
IRP, *PIRP;

// TODO: Also add support for completion ports eventually

// Gets the current IRP stack frame.
PIO_STACK_FRAME IoGetCurrentIrpStackFrame(PIRP Irp);

// Gets the next IRP stack frame.
PIO_STACK_FRAME IoGetNextIrpStackFrame(PIRP Irp);

PIO_STACK_FRAME IoGetIrpStackFrame(PIRP Irp, int Index);

PIRP IoGetIrpFromStackFrame(PIO_STACK_FRAME Frame);

// TODO something something IoCallDriver
// Note: IoCallDriver MUST be appended at the end and it will not actually call into
// the next driver directly, instead it will return and let IoDispatchIrp deal with it
