/***
	The Boron Operating System
	Copyright (C) 2025-2026 iProgramInCpp

Module name:
	ke/arm/trap.S
	
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
	mrs   r12, cpsr
	tst   r12, #(1 << 7)   @ test I bit
	moveq r0, #1           @ 1 if enabled
	movne r0, #0           @ 0 if disabled
#ifdef TARGET_ARMV5
	orr   r12, r12, #0x80   @ set I bit
	msr   cpsr_c, r12
#else
	cpsid if
#endif
	bx    lr

	@ void KeRestoreInterrupts(bool OldState);
	.global KeRestoreInterrupts
KeRestoreInterrupts:
	cmp   r0, #0
	beq   1f
#ifdef TARGET_ARMV5
	mrs   r0, cpsr
	bic   r0, r0, #0x80   @ clear I bit
#else
	cpsie if
#endif
1:	bx    lr

	@ NO_RETURN void KeDescendIntoUserMode(
	@     void* InstructionPointer,
	@     void* StackPointer,
	@     void* UserContext
	@ );
	.global KeDescendIntoUserMode
KeDescendIntoUserMode:
	@ Enter system mode.
	cps #0x1F
	
	@ Prepare SP, LR, and R0.
	mov sp, r1
	mov lr, r0
	mov r0, r2
	
	@ Prepare SPSR to enter user mode.
	@ M = 0x10, A = 1 (imprecise abort)
	mov r1, #0x110
	msr spsr_cxsf, r1
	
	@ Finally, enter user mode.
	movs pc, lr

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
	mov   sp, r0
	ldmia sp!, {r4-r12, lr}
	bx    lr

	@ void KiThreadEntryPoint([register r4] ThreadEntryPoint, [register r5] ThreadContext);
	.global KiThreadEntryPoint
	.extern KiUnlockDispatcher
KiThreadEntryPoint:
	mov   r0, #0
	bl    KiUnlockDispatcher  @ unlock the dispatcher and lower to normal IPL
	mov   r0, r5
	cpsie if                  @ enable interrupts now
	bx    r4

	@ void KiSaveInterruptStacks(
	@     uintptr_t* AbortStack,
	@     uintptr_t* UndefinedStack,
	@     uintptr_t* IrqStack,
	@     uintptr_t* FiqStack
	@ );
	.global KiSaveInterruptStacks
KiSaveInterruptStacks:

#ifdef TARGET_ARMV5
	#warning TODO: update this to match armv6 behavior
	mrs   r12, cpsr
	orr   r12, r12, #0xC0  @ set I and F bits
	bic   r12, r12, #0x1F
	orr   r12, r12, #0x17  @ abort mode
	msr   cpsr_c, r12
	str   sp, [r0]

	bic   r12, r12, #0x1F
	orr   r12, r12, #0x1B  @ undefined mode
	msr   cpsr_c, r12
	str   sp, [r1]

	bic   r12, r12, #0x1F
	orr   r12, r12, #0x12  @ IRQ mode
	msr   cpsr_c, r12
	str   sp, [r2]

	bic   r12, r12, #0x1F
	orr   r12, r12, #0x11  @ FIQ mode
	msr   cpsr_c, r12
	str   sp, [r3]

	bic   r12, r12, #0xDF  @ clear I and F bits as well as mode bits (0xC0 | 0x1F)
	orr   r12, r12, #0x13  @ supervisor mode
	msr   cpsr_c, r12
#else
	cps   #0x17  @ Abort Mode
	str   sp, [r0]
	cps   #0x1B  @ Undefined Mode
	str   sp, [r1]
	cps   #0x12  @ IRQ Mode
	str   sp, [r2]
	cps   #0x11  @ FIQ Mode
	str   sp, [r3]
	cps   #0x13  @ Supervisor Mode
#endif
	bx    lr

	@ void KiRestoreInterruptStacks(
	@     uintptr_t AbortStack,
	@     uintptr_t UndefinedStack,
	@     uintptr_t IrqStack,
	@     uintptr_t FiqStack
	@ );
	.global KiRestoreInterruptStacks
KiRestoreInterruptStacks:

#ifdef TARGET_ARMV5
	#warning TODO: update this to match armv6 behavior
	mrs   r12, cpsr
	orr   r12, r12, #0xC0  @ set I and F bits
	bic   r12, r12, #0x1F
	orr   r12, r12, #0x17  @ abort mode
	msr   cpsr_c, r12
	mov   sp, r0

	bic   r12, r12, #0x1F
	orr   r12, r12, #0x1B  @ undefined mode
	msr   cpsr_c, r12
	mov   sp, r1

	bic   r12, r12, #0x1F
	orr   r12, r12, #0x12  @ IRQ mode
	msr   cpsr_c, r12
	mov   sp, r2

	bic   r12, r12, #0x1F
	orr   r12, r12, #0x11  @ FIQ mode
	msr   cpsr_c, r12
	mov   sp, r3

	bic   r12, r12, #0xDF  @ clear I and F bits as well as mode bits (0xC0 | 0x1F)
	orr   r12, r12, #0x13  @ supervisor mode
	msr   cpsr_c, r12
#else
	cps   #0x17  @ Abort Mode
	mov   sp, r0
	cps   #0x1B  @ Undefined Mode
	mov   sp, r1
	cps   #0x12  @ IRQ Mode
	mov   sp, r2
	cps   #0x11  @ FIQ Mode
	mov   sp, r3
	cps   #0x13  @ Supervisor Mode
#endif
	bx    lr

#ifdef TARGET_ARMV5

#warning TODO: rewrite this for armv5 too

.macro ABORT_HANDLER name, handle, ps
	.global \name
	.extern \handle
\name:
	push  {r0-r7,lr}                 @ push GPRs and the LR. r8-r12 are saved by the C code
	mrs   r0, spsr                   @ prepare CPSR for saving
	push  {r0}
	mov   r0, sp                     @ prepare register pointer
	mrs   r1, cpsr
	bic   r1, r1, #0x1F
	orr   r1, r1, #0x13              @ switch to supervisor mode for this next call
	msr   cpsr_c, r1
	bl    \handle                    @ handle the fault here
	mrs   r1, cpsr
	bic   r1, r1, #0x1F
	orr   r1, r1, #\ps               @ back to the specific mode
	msr   cpsr_c, r1
	pop   {r0}
	msr   spsr, r0                   @ restore old CPSR
	pop   {r0-r7,lr}                 @ restore GPRs and the LR
	subs  pc, lr, #4                 @ and finally return
.endm

#else

.macro ABORT_HANDLER name, handle, ps
	.global \name
	.extern \handle
\name:
	push  {r0-r7,lr}                 @ push GPRs and the LR. r8-r12 are saved by the C code
	mrs   r0, spsr                   @ prepare CPSR for saving
	push  {r0}
	mov   r0, sp                     @ prepare register pointer
	cps   #0x13                      @ switch to supervisor mode for this next call
	bl    \handle                    @ handle the fault here
	cps   #\ps                       @ back to the specific mode
	pop   {r0}
	msr   spsr, r0                   @ restore old CPSR
	pop   {r0-r7,lr}                 @ restore GPRs and the LR
	subs  pc, lr, #4                 @ and finally return
.endm

#endif

@ABORT_HANDLER KiPrefetchAbortHandler, KiHandleInstructionFault, 0x17
@ABORT_HANDLER KiDataAbortHandler, KiHandleDataFault, 0x17
@ABORT_HANDLER KiUndefinedInstructionHandler, KeOnUndefinedInstruction, 0x17
@ABORT_HANDLER KiIrqHandler, HalOnInterruptRequest, 0x12
@ABORT_HANDLER KiFiqHandler, HalOnFastInterruptRequest, 0x11

	.global KiIrqHandler
KiIrqHandler:
	sub   lr, lr, #4
	push  {r0-r12, lr}
	mrs   r0, spsr
	push  {r0}
	push  {r0}  @ dummy to push lr_svc
	mov   r0, sp
	cps   #0x13
	str   lr, [r0]
	bl    HalOnInterruptRequest
	ldr   lr, [r0]
	cps   #0x12
	pop   {r0}
	pop   {r0}
	msr   spsr, r0
	pop   {r0-r12, lr}
	movs  pc, lr
	
	.global KiFiqHandler
KiFiqHandler:
	sub   lr, lr, #4
	push  {r0-r12, lr}
	mrs   r0, spsr
	push  {r0}
	push  {r0}  @ dummy to push lr_svc
	mov   r0, sp
	cps   #0x13
	str   lr, [r0]
	bl    HalOnFastInterruptRequest
	ldr   lr, [r0]
	cps   #0x11
	pop   {r0}
	pop   {r0}
	msr   spsr, r0
	pop   {r0-r12, lr}
	movs  pc, lr
	
	.global KiPrefetchAbortHandler
KiPrefetchAbortHandler:
	sub   lr, lr, #4
	push  {r0-r12, lr}
	mrs   r0, spsr
	push  {r0}
	push  {r0}  @ dummy to push lr_svc
	mov   r0, sp
	cps   #0x13
	str   lr, [r0]
	bl    KiHandleInstructionFault
	ldr   lr, [r0]
	cps   #0x17
	pop   {r0}
	pop   {r0}
	msr   spsr, r0
	pop   {r0-r12, lr}
	movs  pc, lr
	
	.global KiDataAbortHandler
KiDataAbortHandler:
	sub   lr, lr, #8
	push  {r0-r12, lr}
	mrs   r0, spsr
	push  {r0}
	push  {r0}  @ dummy to push lr_svc
	mov   r0, sp
	cps   #0x13
	str   lr, [r0]
	bl    KiHandleDataFault
	ldr   lr, [r0]
	cps   #0x17
	pop   {r0}
	pop   {r0}
	msr   spsr, r0
	pop   {r0-r12, lr}
	movs  pc, lr
	
	.global KiUndefinedInstructionHandler
KiUndefinedInstructionHandler:
	push  {r0-r12, lr}
	mrs   r0, spsr
	push  {r0}
	push  {r0}  @ dummy to push lr_svc
	mov   r0, sp
	cps   #0x13
	str   lr, [r0]
	bl    KiHandleUndefinedInstructionFault
	ldr   lr, [r0]
	cps   #0x1B
	pop   {r0}
	pop   {r0}
	msr   spsr, r0
	pop   {r0-r12, lr}
	movs  pc, lr

	.global KiSoftwareInterruptHandler
	.extern KiSystemServiceHandler
KiSoftwareInterruptHandler:
	push  {r0-r12, lr}
	mrs   r0, spsr
	push  {r0}
	cps   #0x1F     @ enter system mode temporarily
	mov   r1, sp    @ to back up user SP
	cpsie if, #0x13
	push  {r1}      @ this is the Lr_Svc slot in KREGISTERS, but reuse it
	mov   r0, sp
	bl    KiSystemServiceHandler
	pop   {r1}
	cpsid if, #0x1F @ enter system mode temporarily
	mov   sp, r1    @ to restore user SP
	cps   #0x13
	pop   {r0}
	msr   spsr, r0
	pop   {r0-r12, lr}
	movs  pc, lr
