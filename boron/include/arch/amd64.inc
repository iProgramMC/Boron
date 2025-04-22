; Boron64 - Include file for platform specific assembly stuff
%ifndef NS64_AMD64_INC
%define NS64_AMD64_INC
bits 64

%define SEG_NULL        0x00
%define SEG_RING_0_CODE 0x08
%define SEG_RING_0_DATA 0x10
%define SEG_RING_3_DATA 0x18
%define SEG_RING_3_CODE 0x20

; Clears all registers except RAX and RBP
%macro CLEAR_REGS 0
	xor rbx, rbx
	xor rcx, rcx
	xor rdx, rdx
	xor rdi, rdi
	xor rsi, rsi
	xor r8,  r8
	xor r9,  r9
	xor r10, r10
	xor r11, r11
	xor r12, r12
	xor r13, r13
	xor r14, r14
	xor r15, r15
%endmacro

; magic IPL to avoid popping registers we don't need to pop, since
; this interrupt frame was pushed by KeYieldCurrentThread in kernel mode
%define MAGIC_IPL 0x42424242

%endif ;NS64_AMD64_INC
