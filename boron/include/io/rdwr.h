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

BSTATUS IoReadFile(PIO_STATUS_BLOCK Iosb, PFILE_OBJECT FileObject, void* Buffer, size_t Size, uint64_t FileOffset, bool Cached);

BSTATUS IoWriteFile(PIO_STATUS_BLOCK Iosb, PFILE_OBJECT FileObject, const void* Buffer, size_t Size, uint64_t FileOffset, bool Cached);

BSTATUS IoReadDir(PIO_STATUS_BLOCK Iosb, PFILE_OBJECT FileObject, uint64_t Offset, uint64_t Version, PIO_DIRECTORY_ENTRY DirectoryEntry);

BSTATUS OSReadFile(PIO_STATUS_BLOCK Iosb, HANDLE Handle, uint64_t ByteOffset, void* Buffer, size_t Length, uint32_t Flags);

BSTATUS OSWriteFile(PIO_STATUS_BLOCK Iosb, HANDLE Handle, uint64_t ByteOffset, const void* Buffer, size_t Length, uint32_t Flags, uint64_t* OutSize);

BSTATUS OSOpenFile(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes);

BSTATUS OSGetLengthFile(HANDLE FileHandle, uint64_t* Length);

BSTATUS OSTouchFile(HANDLE Handle, bool IsWrite);

BSTATUS OSGetAlignmentFile(HANDLE Handle, size_t* AlignmentOut);

// TODO for OSWriteFile:
//
// If the file is opened with FILE_FLAG_APPEND_ONLY, then the size of the file after
// the write is returned to OutSize.
