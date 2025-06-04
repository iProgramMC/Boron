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
		
		// ReadDir
		uint64_t NextOffset;
		
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
			void*  Start;  // Start should be aligned to 4096 bytes
			size_t Length; // Length should be a multiple of 4096
		}
		BackingMemory;
		
		// GetAlignmentInfo
		struct
		{
			uint32_t BlockSizeLog;
		}
		AlignmentInfo;
	};
}
IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

enum
{
	IO_SEEK_CUR,
	IO_SEEK_SET,
	IO_SEEK_END,
};

#define IO_MAX_NAME (192)

typedef struct _IO_DIRECTORY_ENTRY
{
	// Null-terminated file name.
	char Name[IO_MAX_NAME];
	
	char Reserved[256 - IO_MAX_NAME];
}
IO_DIRECTORY_ENTRY, *PIO_DIRECTORY_ENTRY;
