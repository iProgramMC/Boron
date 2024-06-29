/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/dirent.h
	
Abstract:
	This header file defines the I/O directory entry structure.
	
Author:
	iProgramInCpp - 29 June 2024
***/
#pragma once

#define IO_MAX_NAME (192)

typedef struct _IO_DIRECTORY_ENTRY
{
	// Null-terminated file name.
	char Name[IO_MAX_NAME];
	
	char Reserved[256 - IO_MAX_NAME];
}
IO_DIRECTORY_ENTRY, *PIO_DIRECTORY_ENTRY;
