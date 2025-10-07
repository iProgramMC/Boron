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
		
		// Dummies to imitate the kernel mode structures below:
		struct
		{
			uint64_t Dummy1;
			uint64_t Dummy2;
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
		}
		ParseDir;
		
		// BackingMemory
		struct
		{
			uintptr_t Start;  // Start is a physical address that should be aligned to 4096 bytes
			size_t    Length; // Length should be a multiple of 4096
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
