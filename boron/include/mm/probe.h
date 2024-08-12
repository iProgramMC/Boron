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
#include <status.h>
#include <ke/mode.h>

// Rationale:
//
// It can be kind of expensive to use an MDL every time we read a non-scalar
// data type from the user.  Most of the time we don't need prolonged access
// to the memory - in fact, the only time we really do need such access is when
// finishing I/O requests, when the device driver writes the read-in data to
// that memory, or writes the data to the disk from that memory.
//
// An MDL makes that job easy because it "pins" the pages into physical memory.
// However, oftentimes it isn't necessary to keep the memory pinned for a longer
// amount of time.
//
// Sometimes it is only necessary to have access to the memory during system
// service processing. In this case, it is sufficient to perform a single probe
// at the beginning, and a safe copy at the end. The idea is, even if another
// thread unmaps the memory during the system service, the final safe copy will
// prevent a crash from kernel mode.

// Probes an address, to check if none of the specified area will cause an
// invalid page fault.  Does not make any attempt to remap or anything.
// An MDL can be used if remapping functionality is needed. See above note
// for details.
BSTATUS MmProbeAddress(void* Address, size_t Length, bool ProbeWrite, KPROCESSOR_MODE AccessMode);

// Performs a memory copy in a safe way. Returns an error code if
// copying was interrupted by an invalid page fault.
//
// If VerifyDest is true, then Address is checked for validity.
// Otherwise, Source is checked for validity.
BSTATUS MmSafeCopy(void* Address, const void* Source, size_t Length, KPROCESSOR_MODE AccessMode, bool VerifyDest);

// Check if the specified address range can be used at all. For example, on AMD64,
// the area between 0x0000800000000000 and 0xFFFF7FFFFFFFFFFF is considered
// noncanonical, and any attempt to access it will actually throw a #GP, not a #PF.
bool MmIsAddressRangeValid(uintptr_t Address, size_t Size, KPROCESSOR_MODE AccessMode);
