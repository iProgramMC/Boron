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

// Performs a direct write operation over an FCB for the modified
// page writer's use.  Only meant for the kernel, should not be used
// by device drivers.

BSTATUS IoPerformModifiedPageWrite(
	PFCB Fcb,
	MMPFN Pfn,
	uint64_t FileOffset
);

#endif

BSTATUS IoReadFileMdl(
	PIO_STATUS_BLOCK Iosb,
	PFILE_OBJECT FileObject,
	PMDL Mdl,
	uint64_t FileOffset,
	bool Cached
);

BSTATUS IoWriteFileMdl(
	PIO_STATUS_BLOCK Iosb,
	PFILE_OBJECT FileObject,
	PMDL Mdl,
	uint64_t FileOffset,
	bool Cached
);

BSTATUS IoReadFile(
	PIO_STATUS_BLOCK Iosb,
	PFILE_OBJECT FileObject,
	void* Buffer,
	size_t Size,
	uint64_t FileOffset,
	bool Cached
);

BSTATUS IoWriteFile(
	PIO_STATUS_BLOCK Iosb,
	PFILE_OBJECT FileObject,
	const void* Buffer,
	size_t Size,
	uint64_t FileOffset,
	bool Cached
);

BSTATUS IoResetDirectoryReadHead(
	PFILE_OBJECT FileObject
);

BSTATUS IoReadDirectoryEntries(
	PIO_STATUS_BLOCK Iosb,
	PFILE_OBJECT FileObject,
	size_t Count,
	PIO_DIRECTORY_ENTRY DirectoryEntries
);

BSTATUS IoReadDirectoryEntry(
	PIO_STATUS_BLOCK Iosb,
	PFILE_OBJECT FileObject,
	PIO_DIRECTORY_ENTRY DirectoryEntry
);

BSTATUS IoDeviceIoControl(
	PFILE_OBJECT FileObject,
	int IoControlCode,
	void* InBuffer,
	size_t InBufferSize,
	void* OutBuffer,
	size_t OutBufferSize
);

// ** SYSTEM SERVICES **

BSTATUS OSReadFile(PIO_STATUS_BLOCK Iosb, HANDLE Handle, uint64_t ByteOffset, void* Buffer, size_t Length, uint32_t Flags);

BSTATUS OSWriteFile(PIO_STATUS_BLOCK Iosb, HANDLE Handle, uint64_t ByteOffset, const void* Buffer, size_t Length, uint32_t Flags, uint64_t* OutSize);

BSTATUS OSOpenFile(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes);

BSTATUS OSGetLengthFile(HANDLE FileHandle, uint64_t* Length);

BSTATUS OSTouchFile(HANDLE Handle, bool IsWrite);

BSTATUS OSGetAlignmentFile(HANDLE Handle, size_t* AlignmentOut);

BSTATUS OSResetDirectoryReadHead(HANDLE FileHandle);

BSTATUS OSReadDirectoryEntries(PIO_STATUS_BLOCK Iosb, HANDLE FileHandle, size_t DirectoryEntryCount, PIO_DIRECTORY_ENTRY DirectoryEntries);

BSTATUS OSDeviceIoControl(HANDLE FileHandle, int IoControlCode, void* InBuffer, size_t InBufferSize, void* OutBuffer, size_t OutBufferSize);
