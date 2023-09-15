; Boron64 - Include file for platform specific assembly stuff
%ifndef NS64_AMD64_INC
%define NS64_AMD64_INC
bits 64

%define SEG_NULL        0x00
%define SEG_RING_0_CODE 0x08
%define SEG_RING_0_DATA 0x10
%define SEG_RING_3_CODE 0x18
%define SEG_RING_3_DATA 0x20

%endif ;NS64_AMD64_INC
