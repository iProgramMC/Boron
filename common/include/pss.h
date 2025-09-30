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
	
	// Standard I/O handles.
	// Note that the initial process launched by libboron.so gets
	// these as zeros.
	HANDLE StandardIO[3];
	
	// TODO: implement environment variables
	
	// TODO: need anything more?
}
PEB, *PPEB;

#define StandardInput  StandardIO[0]
#define StandardOutput StandardID[1]
#define StandardError  StandardID[2]
