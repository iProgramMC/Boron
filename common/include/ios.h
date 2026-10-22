/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ios.h

Abstract:
	This header file contains the publicly exposed structure
	definitions for Boron's I/O Subsystem.

Author:
	iProgramInCpp - 22 April 2025
***/
#pragma once

#include "ioctl.h"

typedef struct _FCB FCB, *PFCB;

// TODO: I think we should actually phase out the status code from here.
// Every system service already returns the status, so putting it here is
// useless.
typedef struct _IO_STATUS_BLOCK
{
	BSTATUS Status;

	union
	{
		// Generic name
		uint64_t Information;

		// ReadFile
		uint64_t BytesRead;

		// WriteFile
		uint64_t BytesWritten;

		// GetAlignmentInfo
		struct
		{
			uint32_t BlockSizeLog;
		}
		AlignmentInfo;

		// ReadDirectoryEntries
		size_t EntriesRead;

		// CreateFile
		uint32_t CreateInformation;

		// Dummies to imitate the kernel mode structures below:
		struct
		{
			uint64_t Dummy1;
			uint64_t Dummy2;
			uint64_t Dummy3;
		}
		Dummy;

		// ReadDir
#ifdef IS_KERNEL_MODE
		struct
		{
			uint64_t NextOffset;  // The next offset of the file.

			// This value is used to check whether or not the directory
			// file changed.
			uint64_t Version;
		}
		ReadDir;

		// ParseDir
		struct
		{
			PFCB FoundFcb;
			const char* ReparsePath;
			uint32_t ResultCode;
		}
		ParseDir;

		// BackingMemory
		struct
		{
			uintptr_t PhysicalAddress;
		}
		BackingMemory;
#endif
	};
}
IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

enum
{
	IO_SEEK_CUR,
	IO_SEEK_SET,
	IO_SEEK_END,
};

//
// Creation dispositions for OSCreateFile.
//
// NOTE: Due to the arrangement of the enum, these can technically make up a
// bitmask. However, some combinations of bits will be rejected with an
// invalid parameter status.
//
enum
{
	// If the file exists, replace it with a new file. Otherwise, create the file.
	IO_SUPERSEDE,
	// If the file exists, report an error, else create it.
	IO_CREATE,
	// If the file exists, open it, else report an error.
	IO_OPEN,
	// If the file exists, open it, else create it.
	IO_CREATE_OR_OPEN,
	// If the file exists, truncate it, else report an error.
	IO_TRUNCATE,
	// If the file exists, truncate it, else create it.
	IO_TRUNCATE_OR_CREATE,
};

//
// Creation results for OSCreateFile.
//
enum
{
	IO_FILE_NO_ACTION_TAKEN,
	IO_FILE_DOES_NOT_EXIST,
	IO_FILE_EXISTS,
	IO_FILE_CREATED,
	IO_FILE_OPENED,
	IO_FILE_OVERWRITTEN,
	IO_FILE_SUPERSEDED
};

// Supported types of files:
enum FILE_TYPE
{
	FILE_TYPE_UNKNOWN,
	FILE_TYPE_FILE,
	FILE_TYPE_DIRECTORY,
	FILE_TYPE_BLOCK_DEVICE,
	FILE_TYPE_CHARACTER_DEVICE,
	FILE_TYPE_PIPE,
	FILE_TYPE_SOCKET,
	FILE_TYPE_SYMBOLIC_LINK
};

#define IO_MAX_NAME (256)

typedef struct _IO_DIRECTORY_ENTRY
{
	// Null-terminated file name.
	char Name[IO_MAX_NAME];

	uint64_t InodeNumber;
	int Type;
}
IO_DIRECTORY_ENTRY, *PIO_DIRECTORY_ENTRY;

// ---- OSCreateFile flags ----

// Create or open a regular file.
#define IO_FILE_REGULAR_FILE      (FILE_TYPE_FILE)

// Create or open a directory.
#define IO_FILE_DIRECTORY_FILE    (FILE_TYPE_DIRECTORY)

// Create or open a symbolic link.
#define IO_FILE_SYMBOLIC_LINK     (FILE_TYPE_SYMBOLIC_LINK)

// The first 4 bits are reserved for a file type.
#define IO_FILE_TYPE_CREATE_MASK  (0xF)

// Passing nothing to OSCreateFile makes it ignore the type of the target file when opening,
// and makes it create a *regular file* when the path doesn't exist.
#define IO_FILE_ANY_TYPE          (FILE_TYPE_UNKNOWN)

// Creates this file handle for sequential access: ByteOffset and IO_RW_SHARED_FILE_OFFSET
// are ignored and OSSeekFile does nothing.
#define IO_FILE_SEQUENTIAL_ONLY   (1 << 4)

// Disables buffered operations on this file.  This flag is incompatible with
// IO_FILE_SEQUENTIAL_ONLY.
//
// Note that if another handle to this file exists (whether from another process or the
// current one), and it's opened in cached mode (i.e. without IO_FILE_DISABLE_BUFFERING),
// the behavior regarding synchronization is undefined.
//
// There are some additional restrictions for this mode.  First, any buffer passed into
// OSReadFile or OSWriteFile must be aligned to the size of a machine word (4 bytes on
// 32-bit, and 8 bytes on 64-bit).  Second, for certain devices, reads and/or writes must
// be performed in units of the native alignment size for that device.  To acquire the
// alignment requirements for the opened device file, use OSGetAlignmentFile.
#define IO_FILE_DISABLE_BUFFERING (1 << 5)

// ---- OSReadFile or OSWriteFile flags ----

// The operation may not block.  If a situation arises where this operation would block, it is immediately ended.
#define IO_RW_NONBLOCK         (1 << 0)

// This flag is set when the write operation is intended to append to the specified file instead of writing to a
// certain position inside it.  It automatically puts the file offset at the end of the file.
#define IO_RW_APPEND           (1 << 1)

// This flag is set when the caller wants to block (if the stream is empty), and not block (if the stream has data).
// Currently supported when reading pipes, terminal objects, and keyboards.
#define IO_RW_NONBLOCK_UNLESS_EMPTY (1 << 2)

// This flag is set when the caller wants the read operation to end early on a new line.  This is currently supported
// for reading pipes and terminal objects.
#define IO_RW_FINISH_ON_NEWLINE     (1 << 3)

// This flag is set when the caller wants to use the kernel's built-in file offset instead of providing its own file
// offset.  This is useful for POSIX compatibility, as two different processes expect to modify the same file offset
// pointer, because the file description (and Boron's file object) is shared and both processes have references to
// the same object.
#define IO_RW_SHARED_FILE_OFFSET    (1 << 4)

#ifdef KERNEL

// This write should terminate the corresponding read operation.  Supported for pipe objects.
#define IO_RW_TERMINATE_READ   (1 << 28)

// Flags for IO_READ_METHOD and IO_WRITE_METHOD:

// The FCB's rwlock is locked exclusively.  If there's a need for the current thread to own the rwlock exclusively,
// and this isn't checked, then the routine must release the lock and re-acquire it exclusive.
//
// Ignored in user mode.
#define IO_RW_LOCKEDEXCLUSIVE  (1 << 29)

// This is paging I/O.  This means memory might be very scarce or downright not available, so memory allocations
// should be avoided.  This may make memory operations slower, but this is a worthy sacrifice considering the situation.
#define IO_RW_PAGING           (1 << 30)

// Forbidden flags for user mode.
#define IO_RW_USER_MODE_FORBIDDEN_FLAGS (\
	IO_RW_TERMINATE_READ | \
	IO_RW_LOCKEDEXCLUSIVE | \
	IO_RW_PAGING \
)

#endif
