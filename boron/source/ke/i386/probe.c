/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/i386/probe.c
	
Abstract:
	This module implements the high level routines for address probing.
	
	Probing a set of addresses checks that they are usable in kernel mode.
	It also brings all of the demand-pages back in to memory. It works by
	attempting to read from / write to the memory. If the memory is not
	accessible, an invalid page fault is raised by hardware. This makes
	probing about as cheap as just copying the raw data.
	
Author:
	iProgramInCpp - 14 October 2025
***/
#include <mm.h>
#include <ke.h>

bool MmIsAddressRangeValid(uintptr_t Address, size_t Size, KPROCESSOR_MODE AccessMode)
{
	// Size=0 is invalid.
	if (Size == 0) {
		DbgPrint("MmIsAddressRangeValid FAILURE: Size 0");
		return false;
	}
	
	// Check for overflow.
	uintptr_t AddressEnd = Address + Size;
	if (AddressEnd < Address) {
		DbgPrint("MmIsAddressRangeValid FAILURE: AddressEnd %p < Address %p", AddressEnd, Address);
		return false;
	}
	
	if (AccessMode == MODE_USER && AddressEnd > MM_USER_SPACE_END) {
		DbgPrint("MmIsAddressRangeValid FAILURE: AccessMode==MODEUSER   AddressEnd %p  Address %p  Size: %zu  RA:%p", AddressEnd, Address, Size, CallerAddress());
		return false;
	}
	
	return true;
}

// Defined in arch/i386/misc.asm
int MmProbeAddressSub(void* Address, size_t Length, bool ProbeWrite);

// This is the front-end for the probing code. The actual probing
// is performed in assembly, because it's impossible to predict what
// kind of stack layout the C version would use. (It could differ
// depending on compiler version, for example.)
BSTATUS MmProbeAddress(void* Address, size_t Length, bool ProbeWrite, KPROCESSOR_MODE AccessMode)
{
	if (!MmIsAddressRangeValid((uintptr_t)Address, Length, AccessMode))
		return STATUS_INVALID_PARAMETER;

	const uintptr_t MaxPtr     = (uintptr_t) ~0ULL; // 0b1111...1111
	const uintptr_t HalfMaxPtr = MaxPtr >> 1;       // 0b0111...1111
	const uintptr_t MSBPtrSet  = ~HalfMaxPtr;       // 0b1000...0000
	
	// If the size is bigger than or equal to MAX_ADDRESS>>1
	if (Length > HalfMaxPtr)
		return STATUS_INVALID_PARAMETER;
	
	uintptr_t AddressLimit = (uintptr_t) Address + Length;
	
	// If the address and the address limit are in
	// different halves of the address space
	if (((uintptr_t)Address ^ AddressLimit) == MSBPtrSet)
		return STATUS_INVALID_PARAMETER;
	
	KeGetCurrentThread()->Probing = true;
	
	int Code = MmProbeAddressSub (Address, Length, ProbeWrite);
	
	if (Code != STATUS_SUCCESS)
	{
		KeGetCurrentThread()->Probing = false;
		return Code;
	}
	
	KeGetCurrentThread()->Probing = false;
	return STATUS_SUCCESS;
}

// Defined in arch/i386/misc.asm
int MmSafeCopySub(void* Address, const void* Source, size_t Length);

BSTATUS MmSafeCopy(void* Address, const void* Source, size_t Length, KPROCESSOR_MODE AccessMode, bool VerifyDest)
{
	if (VerifyDest)
	{
		if (!MmIsAddressRangeValid((uintptr_t)Address, Length, AccessMode))
			return STATUS_INVALID_PARAMETER;
	}
	else
	{
		if (!MmIsAddressRangeValid((uintptr_t)Source, Length, AccessMode))
			return STATUS_INVALID_PARAMETER;
	}
	
	// Let the page fault handler know we are probing.
	KeGetCurrentThread()->Probing = true;
	
	// This is just a regular old memcpy.  Nothing different about it,
	// other than the return value.  It's a five instruction marvel.
	int Code = MmSafeCopySub(Address, Source, Length);
	
	// If it returned through a path different than usual (i.e. it was
	// detoured through MmProbeAddressSubEarlyReturn), then it's going
	// to return STATUS_FAULT, which we'll mirror when returning.
	
	// No longer probing.
	KeGetCurrentThread()->Probing = false;
	
	return Code;
}
