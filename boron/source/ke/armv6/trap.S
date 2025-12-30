/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/armv6/trap.S
	
Abstract:
    This module implements functions related to interrupt management
	for the armv6 architecture.
	
Author:
	iProgramInCpp - 26 December 2025
***/

	.section .text

	@ bool KeDisableInterrupts();
	.global KeDisableInterrupts
KeDisableInterrupts:
	mrs   r0, cpsr
	tst   r0, #(1 << 7)   @ test I bit
	moveq r0, #1          @ 1 if enabled
	movne r0, #0          @ 0 if disabled
	cpsid i
	bx    lr

	@ void KeRestoreInterrupts(bool OldState);
	.global KeRestoreInterrupts
KeRestoreInterrupts:
	cmp   r0, #0
	beq   1f
	cpsie i
1:	bx    lr

	@ void KeOnUpdateIPL(KIPL OldIpl, KIPL NewIpl);
	.global KeOnUpdateIPL
KeOnUpdateIPL:
	bx    lr

	@ NO_RETURN void KeDescendIntoUserMode(
	@     void* InstructionPointer,
	@     void* StackPointer,
	@     void* UserContext,
	@     uintptr_t ReturnCode
	@ );
	.global KeDescendIntoUserMode
	.extern KeCrash
KeDescendIntoUserMode:
	@ TODO: actually implement this
	ldr  r0, =KeDescendIntoUserModeUnimplementedMessage
	bl   KeCrash
	bx   lr
KeDescendIntoUserModeUnimplementedMessage:
	.asciz "NYI KeDescendIntoUserMode"

	@ void KiSwitchThreadStack(void** OldStack, void** NewStack);
	.global KiSwitchThreadStack
KiSwitchThreadStack:
	stmdb sp!, {r4-r12, lr}  @ save callee saved regs for old thread
	str   sp, [r0]
	ldr   sp, [r1]
	ldmia sp!, {r4-r12, lr}  @ restore callee saved regs from new thread
	bx    lr

	@ void KiSwitchThreadStackForever(void* NewStack)
	.global KiSwitchThreadStackForever
KiSwitchThreadStackForever:
	ldr   sp, [r0]
	ldmia sp!, {r4-r12, lr}
	bx    lr

	@ void KiThreadEntryPoint([register r4] ThreadEntryPoint, [register r5] ThreadContext);
	.global KiThreadEntryPoint
	.extern KiUnlockDispatcher
KiThreadEntryPoint:
	mov   r0, #0
	bl    KiUnlockDispatcher  @ unlock the dispatcher and lower to normal IPL
	mov   r0, r5
	bx    r4

/* I'm in the process of rewriting this so that abort state is per thread and not per CPU */
/*
	.global KiPrefetchAbortHandler
	.extern KiHandleInstructionFault
KiPrefetchAbortHandler:
	cpsid if
	push  {lr}                       @ push LR to the abort stack
	mrs   lr, spsr                   @ and also the old CPSR
	push  {lr}
	cps   #0x13                      @ change into supervisor mode
	push  {r0-r7}                    @ push some registers. the C function will preserve the rest too
	ldr   r0, =(KiAbortStackBottom - 8)
	ldmia r0!, {r1, r2}              @ load the LR and old CPSR, and update r0 to the bottom of the stack
	push  {r1, r2}                   @ then push them
	mov   r0, sp                     @ provide pointer to saved registers
	cpsie if                         @ enable interrupts again
	
	bl    KiHandleInstructionFault
	
	cpsid if                         @ disable interrupts now
	pop   {r1, r2}                   @ pop the old LR and CPSR
	ldr   r0, =(KiAbortStackBottom)
	stmdb r0!, {r1, r2}              @ put them inside the abort stack
	cps   #0x17                      @ switch back to ABT mode
	mov   sp, r0
	pop   {lr}                       @ pop and restore old CPSR
	msr   spsr, lr
	pop   {lr}                       @ and then LR
	subs  pc, lr, #4                 @ and finally, return

	.global KiDataAbortHandler
	.extern KiHandleDataFault
KiDataAbortHandler:
	push  {r14}                      @ push LR to the abort stack (R13_ABT is setup at boot)
	mrs   r14, spsr                  @ and then also the old CPSR
	push  {r14}
	cps   #0x13                      @ change to supervisor mode
	push  {r0-r7}                    @ save a bunch of registers. C code will save the rest
	ldr   r2, =(KiAbortStackBottom - 4)
	ldr   r0, [r2]                   @ prepare to load r14
	push  {r0}                       @ then push it to the stack
	mov   r0, sp                     @ provide the page fault handler with a copy of the registers
	bl    KiHandleDataFault
	pop   {r0}                       @ restore old r14
	ldr   r2, =(KiAbortStackBottom - 4)
	str   r0, [r2]                   @ and save it
	pop   {r0-r7}
	cps   #0x17                      @ change to abort mode, then return
	pop   {r14}
	msr   spsr, r14                  @ restore the old CPSR
	pop   {r14}                      @ as well as the old LR
	subs  pc, r14, #4                @ and finally, return!

	.global KiUndefinedInstructionHandler
	.extern KeOnUndefinedInstruction
KiUndefinedInstructionHandler:
	push  {r14}                      @ push LR to the abort stack (R13_UND is setup at boot)
	mrs   r14, spsr                  @ and then also the old CPSR
	push  {r14}
	cps   #0x13                      @ change to supervisor mode
	push  {r0-r7}                    @ save a bunch of registers. C code will save the rest
	ldr   r2, =(KiUndefinedInstructionStackBottom - 4)
	ldr   r0, [r2]                   @ prepare to load r14
	push  {r0}                       @ then push it to the stack
	mov   r0, sp                     @ provide the page fault handler with a copy of the registers
	bl    KeOnUndefinedInstruction
	pop   {r0}                       @ restore old r14
	ldr   r2, =(KiUndefinedInstructionStackBottom - 4)
	str   r0, [r2]                   @ and save it
	pop   {r0-r7}
	cps   #0x1B                      @ change to undefined mode, then return
	pop   {r14}
	msr   spsr, r14                  @ restore the old CPSR
	pop   {r14}                      @ as well as the old LR
	subs  pc, r14, #4                @ and finally, return!

	.global KiSoftwareInterruptHandler
	.extern KeOnSoftwareInterrupt
KiSoftwareInterruptHandler:
	push  {r14}                      @ push LR to the abort stack (R13_UND is setup at boot)
	mrs   r14, spsr                  @ and then also the old CPSR
	push  {r14}
	push  {r0-r7}                    @ save a bunch of registers. C code will save the rest
	push  {r14}
	mov   r0, sp                     @ provide the page fault handler with a copy of the registers
	bl    KeOnSoftwareInterrupt
	pop   {r0}                       @ restore old r14
	ldr   r2, =(KiSoftwareInterruptStackBottom - 4)
	str   r0, [r2]                   @ and save it
	pop   {r0-r7}
	cps   #0x1B                      @ change to undefined mode, then return
	pop   {r14}
	msr   spsr, r14                  @ restore the old CPSR
	pop   {r14}                      @ as well as the old LR
	movs  pc, r14                    @ and finally, return!
*/
