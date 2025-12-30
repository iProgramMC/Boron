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

	@ void KiSaveInterruptStacks(
	@     uintptr_t* AbortStack,
	@     uintptr_t* UndefinedStack,
	@     uintptr_t* IrqStack,
	@     uintptr_t* FiqStack
	@ );
	.global KiSaveInterruptStacks
KiSaveInterruptStacks:
	cpsid if
	cps   #0x17  @ Abort Mode
	str   sp, [r0]
	cps   #0x1B  @ Undefined Mode
	str   sp, [r1]
	cps   #0x12  @ IRQ Mode
	str   sp, [r2]
	cps   #0x11  @ FIQ Mode
	str   sp, [r3]
	cps   #0x13  @ Supervisor Mode
	cpsie if
	bx    lr

	@ void KiRestoreInterruptStacks(
	@     uintptr_t AbortStack,
	@     uintptr_t UndefinedStack,
	@     uintptr_t IrqStack,
	@     uintptr_t FiqStack
	@ );
	.global KiRestoreInterruptStacks
KiRestoreInterruptStacks:
	cpsid if
	cps   #0x17  @ Abort Mode
	mov   sp, r0
	cps   #0x1B  @ Undefined Mode
	mov   sp, r1
	cps   #0x12  @ IRQ Mode
	mov   sp, r2
	cps   #0x11  @ FIQ Mode
	mov   sp, r3
	cps   #0x13  @ Supervisor Mode
	cpsie if
	bx    lr

	.global KiPrefetchAbortHandler
	.extern KiHandleInstructionFault
KiPrefetchAbortHandler:
	push  {r0-r7,lr}                 @ push GPRs and the LR. r8-r12 are saved by the C code
	mrs   r0, spsr                   @ prepare CPSR for saving
	push  {r0}
	mov   r0, sp                     @ prepare register pointer
	cps   #0x13                      @ switch to supervisor mode for this next call
	bl    KiHandleInstructionFault   @ handle the fault here
	cps   #0x17                      @ back to abort mode
	pop   {r0}
	msr   spsr, r0                   @ restore old CPSR
	pop   {r0-r7,lr}                 @ restore GPRs and the LR
	subs  pc, lr, #4                 @ and finally return

	.global KiDataAbortHandler
	.extern KiHandleDataFault
KiDataAbortHandler:
	push  {r0-r7,lr}                 @ push GPRs and the LR. r8-r12 are saved by the C code
	mrs   r0, spsr                   @ prepare CPSR for saving
	push  {r0}
	mov   r0, sp                     @ prepare register pointer
	cps   #0x13                      @ switch to supervisor mode for this next call
	bl    KiHandleDataFault          @ handle the fault here
	cps   #0x17                      @ back to abort mode
	pop   {r0}
	msr   spsr, r0                   @ restore old CPSR
	pop   {r0-r7,lr}                 @ restore GPRs and the LR
	subs  pc, lr, #4                 @ and finally return

	.global KiUndefinedInstructionHandler
	.extern KeOnUndefinedInstruction
KiUndefinedInstructionHandler:
	push  {r0-r7,lr}                 @ push GPRs and the LR. r8-r12 are saved by the C code
	mrs   r0, spsr                   @ prepare CPSR for saving
	push  {r0}
	mov   r0, sp                     @ prepare register pointer
	cps   #0x13                      @ switch to supervisor mode for this next call
	bl    KeOnUndefinedInstruction   @ handle the fault here
	cps   #0x17                      @ back to abort mode
	pop   {r0}
	msr   spsr, r0                   @ restore old CPSR
	pop   {r0-r7,lr}                 @ restore GPRs and the LR
	subs  pc, lr, #4                 @ and finally return

	.global KiSoftwareInterruptHandler
	.extern KeOnSoftwareInterrupt
KiSoftwareInterruptHandler:
	push  {r0-r7,lr}                 @ push GPRs and the LR. r8-r12 are saved by the C code
	mrs   r0, spsr                   @ prepare CPSR for saving
	push  {r0}
	mov   r0, sp                     @ prepare register pointer
	bl    KeOnSoftwareInterrupt      @ handle the call here
	pop   {r0}
	msr   spsr, r0                   @ restore old CPSR
	pop   {r0-r7,lr}                 @ restore GPRs and the LR
	subs  pc, lr, #4                 @ and finally return

	.global KiIrqHandler
	.extern KeOnInterruptRequest
KiIrqHandler:
	push  {r0-r7,lr}                 @ push GPRs and the LR. r8-r12 are saved by the C code
	mrs   r0, spsr                   @ prepare CPSR for saving
	push  {r0}
	mov   r0, sp                     @ prepare register pointer
	cps   #0x13                      @ switch to supervisor mode
	bl    KeOnInterruptRequest       @ handle the IRQ here
	cps   #0x12                      @ back to IRQ mode
	mov   sp, r0
	pop   {r0}
	msr   spsr, r0                   @ restore old CPSR
	pop   {r0-r7,lr}                 @ restore GPRs and the LR
	subs  pc, lr, #4                 @ and finally return

	.global KiFiqHandler
	.extern KeOnFastInterruptRequest
KiFiqHandler:
	push  {r0-r7,lr}                 @ push GPRs and the LR. r8-r12 are saved by the C code
	mrs   r0, spsr                   @ prepare CPSR for saving
	push  {r0}
	mov   r0, sp                     @ prepare register pointer
	cps   #0x13                      @ switch to supervisor mode
	bl    KeOnFastInterruptRequest   @ handle the FIQ here
	cps   #0x11                      @ back to FIQ mode
	mov   sp, r0
	pop   {r0}
	msr   spsr, r0                   @ restore old CPSR
	pop   {r0-r7,lr}                 @ restore GPRs and the LR
	subs  pc, lr, #4                 @ and finally return
