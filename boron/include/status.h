/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	status.h
	
Abstract:
	This header file contains the status code enumeration
	definition for the Boron operating system.
	
Author:
	iProgramInCpp - 30 October 2023
***/
#pragma once

typedef int BSTATUS;

enum
{
	STATUS_SUCCESS,             // Must ALWAYS be defined as zero!
	STATUS_INVALID_PARAMETER,
	STATUS_ACCESS_DENIED,
	STATUS_WAITING,             // these 3 are returned by the KeWaitFor*Object(s) functions.
	STATUS_ALERTED,
	STATUS_TIMEOUT,
	STATUS_FAULT,               // returned by MmProbeAddress when a bad page fault was triggered.
	STATUS_NO_REMAP,            // returned by MmProbeAddress if remapping in kernel space failed.
	STATUS_REPARSE,             // Parse succeeded, but start the parse over at CompleteName.
	STATUS_OBJPATH_SYNTAX_BAD,  // Parse failed because of ill formed path name
	STATUS_OBJPATH_NOT_FOUND,   // Parse failed because a path component was not found
	STATUS_OBJNAME_NOT_FOUND,   // Parse failed because the final path component was not found
	STATUS_OBJPATH_INVALID,     // If parse succeeded and matched an object, but there were more characters to parse.
	
	STATUS_RANGE_WAIT           = 0x1000000, // range 0..MAXIMUM_WAIT_BLOCKS
	STATUS_RANGE_ABANDONED_WAIT = 0x1000040, // range 0..MAXIMUM_WAIT_BLOCKS
};
