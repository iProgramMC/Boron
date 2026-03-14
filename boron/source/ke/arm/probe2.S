/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/armv6/probe.S
	
Abstract:
    This module implements the MmSafeCopySub and MmProbeAddressSub
	functions for the armv6 architecture.
	
Author:
	iProgramInCpp - 26 December 2025
***/

	.section .text
	
	@ TODO: optimize this.  Currently it does byte-by-byte copies, but this
	@ is bad.  However, typically we don't need to copy too much data at once
	@ so this is fine for now.
	
	@ void MmSafeCopySub(void* Destination, const void* Source, size_t ByteCount);
	.global MmSafeCopySub
MmSafeCopySub:
	cmp  r2, #0
	beq  2f
1:
	ldrb r3, [r1], #1   @ load a byte from src
	strb r3, [r0], #1   @ store byte to dst
	subs r2, r2, #1
	bne  1b
2:
	mov  r0, #0
	bx   lr

	@ int MmProbeAddressSub(void* Address, size_t Length, bool ProbeWrite);
	.global MmProbeAddressSub
MmProbeAddressSub:
	add  r3, r0, r1   @ AddressEnd = Address + Length
	cmp  r0, r3
	bhs 3f
1:
	ldrb r4, [r0]     @ probe the read
	cmp  r2, #0       @ check if need to write
	beq  2f
	strb r4, [r0]     @ probe the write
2:
	add  r0, r0, #4096 @ add a page size
	cmp  r0, r3
	blo  1b           @ Address < AddressEnd
3:
	mov  r0, #0
	bx   lr

	@ Returns early from MmProbeAddressSub and MmSafeCopySub.
	@ Called by the invalid page fault handler.
	.global MmProbeAddressSubEarlyReturn
MmProbeAddressSubEarlyReturn:
	bx   lr
