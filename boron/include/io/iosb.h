/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/iosb.h
	
Abstract:
	This header file defines the I/O status block structure.
	
Author:
	iProgramInCpp - 29 June 2024
***/
#pragma once
#include <status.h>

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
		
		// LookUpDir
		PFCB FoundFcb;
	};
}
IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

