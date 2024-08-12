/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	arch/amd64/probe.c
	
Abstract:
	This module implements the high level routines for address probing.
	
	Probing a set of addresses checks that they are usable in kernel mode.
	It also brings all of the demand-pages back in to memory. It works by
	attempting to read from / write to the memory. If the memory is not
	accessible, an invalid page fault is raised by hardware. This makes
	probing extremely cheap.
	
Author:
	iProgramInCpp - 25 November 2023
***/
#include <mm.h>
#include <ke.h>

// NOTE: On AMD64, we *have* to do this. Accesses to non-canonical addresses return
// a general protection fault, instead of a page fault. We aren't set up to handle
// probing using those, not that we'd want to anyway.

bool MmIsAddressCanonical(uintptr_t Address)
{
	int64_t AddressSigned = (int64_t) Address;
	int64_t SignExt = AddressSigned >> 47;
	
	// The logic is that we convert the address to a signed integer which we then
	// shift right by 47 bits, smearing the upper 47 bits with the 63rd bit of the
	// old value. Thus, if the address is canonical, this is either 0xFFFF..FFFF or
	// 0x0000..0000.  If it's anything else, the upper 16 bits of the address don't
	// match the upper 17th, which means the address is non-canonical.
	
	return (~SignExt == 0) || (SignExt == 0);
}

bool MmIsAddressRangeValid(uintptr_t Address, size_t Size, KPROCESSOR_MODE AccessMode)
{
	// Size=0 is invalid.
	if (Size == 0)
		return false;
	
	// Check for overflow.
	uintptr_t AddressEnd = Address + Size;
	if (AddressEnd < Address)
		return false;
	
	if (AccessMode == MODE_USER && AddressEnd > MM_USER_SPACE_END)
		return false;
	
	// If the upper 17 bits aren't identical, then the address crosses over into
	// non-canonical space. Since the addresses are unsigned, the upper bit won't
	// get "smeared" - not like that matters anyway :)
	return (Address >> 17) == (AddressEnd >> 17);
}

// Defined in arch/amd64/misc.asm
int MmProbeAddressSub(void* Address, size_t Length, bool ProbeWrite);

// This is the front-end for the probing code. The actual probing
// is performed in assembly, because it's impossible to predict what
// kind of stack layout the C version would use. (It could differ
// depending on compiler version, for example.)
BSTATUS MmProbeAddress(void* Address, size_t Length, bool ProbeWrite, KPROCESSOR_MODE AccessMode)
{
#ifdef DEBUG
	// Some of the parameters are just not valid for now.
	if (Length % sizeof(uint32_t))
		KeCrash("MmProbeAddress: Length %zu not aligned to %zu bytes", Length, sizeof(uint32_t));
#endif
	
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

// Defined in arch/amd64/misc.asm
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
