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

typedef struct _IRP
{
	uint8_t  FunctionID;
	uint8_t  Avl0;
	uint16_t Avl1;
	uint32_t Flags;
	
	// Mode that requested this operation.
	KPROCESSOR_MODE ModeRequestor;
	
	// Buffer associated with the IRP.  This is the
	// source/destination buffer that the IRP uses
	// to transfer data.
	PMDL Mdl;
	
	// An event that gets signaled when the IRP is completed.
	KEVENT CompletionEvent;
}
IRP, *PIRP;
