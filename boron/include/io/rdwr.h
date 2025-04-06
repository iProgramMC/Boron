/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/rdwr.h
	
Abstract:
	This header defines the I/O manager's user-facing
	I/O functions, such as reading and writing.
	
Author:
	iProgramInCpp - 2 July 2024
***/
#pragma once

#include <ob.h>
#include "iosb.h"

#ifdef KERNEL

// Performs a direct read operation over a file object.  This is only
// for use by the kernel and should not be used by device drivers.

BSTATUS IoPerformPagingRead(
	PIO_STATUS_BLOCK Iosb,
	PFILE_OBJECT FileObject,
	PMDL Mdl,
	uint64_t FileOffset
);

#endif

BSTATUS IoReadFile(PIO_STATUS_BLOCK Iosb, HANDLE Handle, PMDL Mdl, uint32_t Flags);

BSTATUS IoWriteFile(PIO_STATUS_BLOCK Iosb, HANDLE Handle, PMDL Mdl, uint32_t Flags);

BSTATUS IoTouchFile(HANDLE Handle, bool IsWrite);

BSTATUS IoGetAlignmentInfo(HANDLE Handle, size_t* AlignmentOut);

BSTATUS OSReadFile(PIO_STATUS_BLOCK Iosb, HANDLE Handle, void* Buffer, size_t Length, uint32_t Flags);

BSTATUS OSWriteFile(PIO_STATUS_BLOCK Iosb, HANDLE Handle, const void* Buffer, size_t Length, uint32_t Flags);

BSTATUS OSOpenFile(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes);
