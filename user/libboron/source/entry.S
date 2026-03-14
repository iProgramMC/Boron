// The Boron Operating System
// Copyright (C) 2026 iProgramInCpp

.section .text

#ifdef TARGET_AMD64
	.att_syntax

	/* void OSDLLJumpToEntry(uintptr_t StackBottom, void* EntryPoint, void* Context); */
	.globl OSDLLJumpToEntry
	.type  OSDLLJumpToEntry, @function
OSDLLJumpToEntry:
	mov %rdi, %rsp
	mov %rdx, %rdi
	xor %ebp, %ebp
	jmp *%rsi

	/* Entry point of libboron.so.  RSP is already aligned to 16 but RelocateSelf
	   requires us to do things a bit differently, seemingly. */
	.globl  OSDLLEntry
	.extern RelocateSelf
	.type   OSDLLEntry, @function
OSDLLEntry:
	call    OSDLLRelocateSelf /* note: this function is NORETURN */

#elif defined TARGET_I386
	.att_syntax

	/* void OSDLLJumpToEntry(uintptr_t StackBottom, void* EntryPoint, void* Context); */
	.globl OSDLLJumpToEntry
	.type  OSDLLJumpToEntry, @function
OSDLLJumpToEntry:
	movl 4(%esp),  %eax  /* StackBottom */
	movl 8(%esp),  %ebx  /* EntryPoint */
	movl 12(%esp), %ecx  /* Context */
	mov  %eax, %esp
	xor  %ebp, %ebp
	push %ecx
	call *%ebx

	/* Entry point of libboron.so. */
	.globl  OSDLLEntry
	.extern OSDLLRelocateSelf
	.type   OSDLLEntry, @function
OSDLLEntry:
	jmp     OSDLLRelocateSelf

#elif TARGET_ARM

#warning Implement this TODO

	.globl OSDLLJumpToEntry
	.type  OSDLLJumpToEntry, %function
OSDLLJumpToEntry:
	bx  lr

#else

#error Implement this TODO

#endif
