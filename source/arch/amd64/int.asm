; Boron64 - Interrupts
bits 64

%include "arch/amd64.inc"

%macro PUSHSTATE 0
	push rax
	push rbx
	push rcx
	push rdx
	push rsi
	push rdi
	push rbp
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15
	mov  rax, cr2
	push rax
	mov  ax,  gs
	push ax        ; 16-bit push
	mov  ax,  fs
	push ax
	mov  ax,  es
	push ax
	mov  ax,  ds
	push ax
	cmp  ax,  SEG_RING_0_DATA  ; check if DS is the kernel's data segment
	je   .pushstate_dontswapgs ; if it isn't, swap to the kernel GS
	swapgs
.pushstate_dontswapgs:
	nop
%endmacro

%macro POPSTATE 0
	pop  ax
	cmp  ax,  SEG_RING_0_DATA  ; check if DS is the kernel's data segment
	je   .popstate_dontswapgs  ; if it isn't, swap to the kernel GS
	swapgs
.popstate_dontswapgs:
	mov  ds, ax
	pop  ax
	mov  es, ax
	pop  ax
	mov  fs, ax
	pop  ax
	mov  gs, ax
	add  rsp, 8  ; pop cr2
	pop  r15
	pop  r14
	pop  r13
	pop  r12
	pop  r11
	pop  r10
	pop  r9
	pop  r8
	pop  rbp
	pop  rdi
	pop  rsi
	pop  rdx
	pop  rcx
	pop  rbx
	pop  rax
%endmacro

%macro BASEINT 3
extern Ki%3Handler
global Ki%1
Ki%1:
	%ifidn %2, PUSH
	push qword 0
	%endif
	PUSHSTATE
	mov  rdi, rsp
	call Ki%3Handler
	POPSTATE
	add  rsp, 8   ; get rid of the error code
	iretq
%endmacro

%macro TRAP_ENTRY 2
BASEINT Trap%1, %2, Trap%1
%endmacro

BASEINT TrapUnknown, PUSH, TrapUnknown
TRAP_ENTRY 08, DONTPUSH
TRAP_ENTRY 0D, DONTPUSH
TRAP_ENTRY 0E, DONTPUSH