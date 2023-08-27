; Boron - CPU abstraction functions
bits 64

%include "hal/amd64.inc"

; these functions set the CR3
global HalSetCurrentPageTable
global HalGetCurrentPageTable

; void* HalGetCurrentPageTable()
HalGetCurrentPageTable:
	mov rax, cr3
	ret

; void HalSetCurrentPageTable(void* pt)
HalSetCurrentPageTable:
	mov cr3, rdi
	ret

global HalOnUpdateIPL

; void HalOnUpdateIPL(eIPL oldIPL, eIPL newIPL)
HalOnUpdateIPL:
	mov cr8, rsi
	ret

; At this point, RAX contains the interrupt handler type.
; We already preserved RAX.
HalInterruptEntry:
	; Push the int type, then the rest of the registers.
	push rax
	mov  rax, [HalInterruptHandlers + 8 * rax] ; resolve the interrupt handler
	test rax, rax
	jz   .end  ; if the interrupt handler is zero, go straight to the part where we're about to exit. Probably not a great idea?
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
	mov  rbx, cr2 ; why in rbx? we are using rax..
	push rbx
	mov  bx, gs
	push bx
	mov  bx, fs
	push bx
	mov  bx, es
	push bx
	mov  bx, ds
	push bx

	; call the actual interrupt handler!
	mov  rdi, rsp   ; set the first argument as the state parameter
	call rax        ; call the ISR itself.
	
	pop  bx
	mov  ds, bx
	pop  bx
	mov  es, bx
	pop  bx
	mov  fs, bx
	pop  bx
	mov  gs, bx
	add  rsp,  8  ; skip cr2
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
.end:
	pop  rax      ; pop the interrupt type
	pop  rax      ; pop the old value of RAX
	add  rsp,  8  ; pop and discard the error code
	iretq         ; Return from the interrupt!!

global HalpDoubleFaultHandler
global HalpPageFaultHandler
global HalpDpcIpiHandler
global HalpClockIrqHandler

HalpDoubleFaultHandler:
	; error code already pushed
	push rax                   ; rax already pushed
	mov  rax, INT_DOUBLE_FAULT ; interrupt type
	jmp  HalInterruptEntry     ; handles the rest including returning

HalpPageFaultHandler:
	; error code already pushed
	push rax                   ; rax already pushed
	mov  rax, INT_PAGE_FAULT   ; interrupt type
	jmp  HalInterruptEntry     ; handles the rest including returning

HalpDpcIpiHandler:
	push 0                     ; fake error code
	push rax                   ; rax already pushed
	mov  rax, INT_DPC_IPI      ; interrupt type
	jmp  HalInterruptEntry     ; handles the rest including returning

HalpClockIrqHandler:
	push 0                     ; fake error code
	push rax                   ; rax already pushed
	mov  rax, INT_DPC_IPI      ; interrupt type
	jmp  HalInterruptEntry     ; handles the rest including returning


; Here's how a normal interrupt handler would look like
;global HalExampleInterruptHandler
;HalExampleInterruptHandler:
;	push 0                 ; error code
;	push rax               ; preserve RAX
;	mov rax, INT_UNKNOWN   ; just an example
;	jmp  HalInterruptEntry ; everything else is handled here. This keeps
;	                       ; the individual interrupt handlers small.
;;end

; And here's also an example of how the interrupt handler would look on the C-side. Pretty simple, don't you think?!
; void KeExampleInterruptHandler(CPUState* pState)
; {
; 	eIPL old_ipl = KeRaiseIPL(myiplhere); // in the case of page faults, APC level. Depends on the device
; 	
; 	// do all your interrupt shenanigans here
; 	
; 	KeLowerIPL(old_ipl); // also sends a self-IPI to dispatch DPCs
; }

; void HalpLoadGdt(GdtDescriptor* desc);
global HalpLoadGdt
HalpLoadGdt:
	mov rax, rdi
	lgdt [rax]
	
	; update the code segment
	push 0x08           ; code segment
	lea rax, [rel .a]   ; jump address
	push rax
	retfq               ; return far - will go to .a now
.a:
	; update the segments
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	ret

; void HalpLoadTss(int descriptor)
global HalpLoadTss
HalpLoadTss:
	mov ax, di
	ltr ax
	ret


section .bss

; interrupt handlers list
global HalInterruptHandlers
HalInterruptHandlers:
	resq INT_COUNT
