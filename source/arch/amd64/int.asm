; Boron64 - Interrupts
bits 64

%include "arch/amd64.inc"

; int KiEnterHardwareInterrupt(int IntNo);
extern KiEnterHardwareInterrupt
; void KiExitHardwareInterrupt(int OldIpl);
extern KiExitHardwareInterrupt

; To define the handler for an interrupt, you define a function with the prototype:
; CPUState*  KiTrap??Handler(CPUState*);
; This can return the CPU state provided verbatim, or it can return a completely different CPU state.
; This is useful for scheduling.

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

%macro BASEINT 4
extern Ki%3Handler
global Ki%1
Ki%1:
	%ifidn %2, PUSH
	push qword 0
	%endif
	PUSHSTATE           ; Push all members of the KREGISTERS except for the OldIpl
	mov  rdi, %4        ; Update IPL
	call KiEnterHardwareInterrupt
	push rax            ; Push old IPL
	mov  rdi, rsp
	call Ki%3Handler
	mov  rsp, rax
	pop  rdi
	call KiExitHardwareInterrupt
	POPSTATE
	add  rsp, 8         ; get rid of the error code
	iretq
%endmacro

%macro TRAP_ENTRY 3
BASEINT Trap%1, %2, Trap%1, %3
%endmacro

BASEINT TrapUnknown, PUSH, TrapUnknown, 0xF
TRAP_ENTRY 08, DONTPUSH, -1   ; double fault
TRAP_ENTRY 0D, DONTPUSH, -1   ; general protection fault
TRAP_ENTRY 0E, DONTPUSH, -1   ; page fault
TRAP_ENTRY 40, PUSH    , 0x4  ; DPC IPI
TRAP_ENTRY F0, PUSH    , 0xF  ; APIC timer interrupt
TRAP_ENTRY FD, PUSH    , 0xF  ; TLB shootdown IPI
TRAP_ENTRY FE, PUSH    , 0xF  ; crash IPI
TRAP_ENTRY FF, PUSH    , 0xF  ; spurious interrupt

; Arguments:
; rdi - Register state as in KREGISTERS
global KeJumpContext
KeJumpContext:
	mov  rsp, rdi
	POPSTATE
	add  rsp, 8    ; the KREGISTERS struct contains a dummy error code which we don't use..
	iretq

; Return Value:
; rax - Old interrupt state
global KeDisableInterrupts
KeDisableInterrupts:
	pushfq               ; push RFLAGS register
	pop  rax             ; pop it into RAX
	and  rax, 0x200      ; check the previous interrupt flag (1 << 9)
	shr  rax, 9          ; shift right by 9
	cli                  ; disable interrupts
	ret                  ; return rax

; Arguments:
; rdi - Old interrupt state
global KeRestoreInterrupts
KeRestoreInterrupts:
	test rdi, rdi        ; check if old state is zero
	jz   .ret            ; if it is, return immediately
	sti                  ; restore interrupts
.ret:
	ret                  ; done
