/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	mm/probe.h
	
Abstract:
	This header file defines the high level routine for address probing.
	
Author:
	iProgramInCpp - 25 November 2023
***/
#pragma once

#include <main.h>

int MmProbeAddress(
	void* Address,
	size_t Length,
	bool ProbeWrite,
	bool Remap,
	void** RemappedOut);
