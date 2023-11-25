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
	STATUS_WAITING,             // these 3 are returned by the KeWaitFor*Object(s) functions.
	STATUS_ALERTED,
	STATUS_TIMEOUT,
	STATUS_FAULT,               // returned by MmProbeAddress when a bad page fault was triggered.
	STATUS_NO_REMAP,            // returned by MmProbeAddress if remapping in kernel space failed.
	
	STATUS_RANGE_WAIT           = 0x1000000, // range 0..MAXIMUM_WAIT_BLOCKS
	STATUS_RANGE_ABANDONED_WAIT = 0x1000040, // range 0..MAXIMUM_WAIT_BLOCKS
};
