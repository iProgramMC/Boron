/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/fcb.h
	
Abstract:
	This header defines the I/O manager's part of the
	File Control Block structure.  The I/O manager takes
	a relatively hands-off approach to file control, as
	can be seen by the relatively few fields its FCB
	struct features.  Control over management is instead
	surrendered to the File System Driver (FSD).
	
Author:
	iProgramInCpp - 22 June 2024
***/
#pragma once

#include <ex/rwlock.h>

// Supported types of files:
enum FILE_TYPE
{
	FILE_TYPE_UNKNOWN,
	FILE_TYPE_FILE,
	FILE_TYPE_DIRECTORY,
	FILE_TYPE_BLOCK_DEVICE,
	FILE_TYPE_CHARACTER_DEVICE,
};

typedef struct _FCB
{
	PDEVICE_OBJECT DeviceObject;
	
	PIO_DISPATCH_TABLE DispatchTable;
	
	EX_RW_LOCK RwLock;
	
	// FILE_TYPE
	uint8_t FileType;
	
	uint32_t Flags;
	
	// FSD specific extension.  When the FCB is initialized, the
	// size of this extension may be specified.
	size_t ExtensionSize;
	char Extension[];
}
FCB, *PFCB;

// Allocates an FCB.  
PFCB IoAllocateFcb(PDEVICE_OBJECT DeviceObject, size_t ExtensionSize, bool NonPaged);

// Frees an FCB.
void IoFreeFcb(PFCB Fcb);
