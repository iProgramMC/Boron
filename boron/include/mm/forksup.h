/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/forksup.h
	
Abstract:
	This header provides prototypes for memory manager functions
	pertaining to process forking support (symmetric CoW et al.)
	
Author:
	iProgramInCpp - 16 December 2025
***/
#pragma once

typedef struct EPROCESS_tag EPROCESS, *PEPROCESS;

BSTATUS MmCloneAddressSpace(PEPROCESS DestinationProcess);
