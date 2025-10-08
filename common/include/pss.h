/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	pss.h
	
Abstract:
	This header file contains the publicly exposed structure
	definitions for Boron's Process Subsystem.
	
Author:
	iProgramInCpp - 22 April 2025
***/
#pragma once

// Loader information.  This information will be used by libboron.so
// (or another interpreter) when launching the executable.
//
// NOTE: This relies on the executable being mapped in such a way that
// the file header and program headers are mapped. However, this *is*
// usually the case for dynamically linked binaries.
typedef struct
{
	// Did the interpreter already run?
	// This is used when libboron.so is not the primary interpreter
	// (so that libboron.so's entry point can just exit)
	bool RanInterpreter;
	
	// Is the main image already mapped?
	bool MappedImage;
	
	// Information about the mapped image.
	uintptr_t ImageBase;
	void* FileHeader;
	void* ProgramHeaders;
	const char* Interpreter;
}
LOADER_INFORMATION;

typedef struct
{
	// Size to call free with when you need to free the PEB.
	// (I don't know why you would, but the kernel allows you
	// to do it!)
	size_t PebFreeSize;
	
	// Execution details
	char*  ImageName;
	size_t ImageNameSize;
	char*  CommandLine;
	size_t CommandLineSize;
	char*  Environment;
	size_t EnvironmentSize;
	
	LOADER_INFORMATION Loader;
	
	// Standard I/O handles.
	// Note that the initial process launched by libboron.so gets
	// these as zeros.
	HANDLE StandardIO[3];
	
	// Starting directory.
	// This handle should be set to HANDLE_NONE by the interpreter
	// once transferred to the main thread's TEB.
	HANDLE StartingDirectory;
	
	// TODO: implement environment variables
	
	// TODO: need anything more?
}
PEB, *PPEB;

enum
{
	FILE_STANDARD_INPUT,
	FILE_STANDARD_OUTPUT,
	FILE_STANDARD_ERROR
};

#define StandardInput  StandardIO[FILE_STANDARD_INPUT]
#define StandardOutput StandardIO[FILE_STANDARD_OUTPUT]
#define StandardError  StandardIO[FILE_STANDARD_ERROR]
