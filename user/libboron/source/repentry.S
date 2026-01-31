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

#elif TARGET_ARM

#warning Implement this TODO

	.globl OSDLLJumpToEntry
	.type  OSDLLJumpToEntry, %function
OSDLLJumpToEntry:
	bx  lr

#else

#error Implement this TODO

#endif
