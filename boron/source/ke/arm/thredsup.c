/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/arm/thredsup.c
	
Abstract:
	This module implements the architecture specific
	thread state setup routine.
	
Author:
	iProgramInCpp - 29 December 2025
***/
#include "../ki.h"
#include <string.h>

typedef struct {
	uint32_t R4;
	uint32_t R5;
	uint32_t R6;
	uint32_t R7;
	uint32_t R8;
	uint32_t R9;
	uint32_t R10;
	uint32_t R11;
	uint32_t R12;
	uint32_t Lr;
}
ARM_INIT_CONTEXT, *PARM_INIT_CONTEXT;

NO_RETURN void KiThreadEntryPoint();

void KiSetupRegistersThread(PKTHREAD Thread)
{
	uintptr_t StackBottom = ((uintptr_t) Thread->Stack.Top + (uintptr_t) Thread->Stack.Size - 0x10) & ~0xF;
	
	PARM_INIT_CONTEXT Context = (void*)(StackBottom - sizeof(ARM_INIT_CONTEXT));
	memset(Context, 0, sizeof Context);
	
	// R4 contains the thread entry point and R5 contains the context.
	Context->R4 = (uint32_t) Thread->StartRoutine;
	Context->R5 = (uint32_t) Thread->StartContext;
	Context->Lr = (uint32_t) KiThreadEntryPoint;
	
	Thread->StackPointer = (void*) Context;
	
	memset(&Thread->ArchContext, 0, sizeof Thread->ArchContext);
	
	uintptr_t IntStack = (uintptr_t) Thread->InterruptStack;
	Thread->ArchContext.AbtStack = IntStack + KERNEL_INTERRUPT_STACK_SIZE * 1 / 4;
	Thread->ArchContext.UndStack = IntStack + KERNEL_INTERRUPT_STACK_SIZE * 2 / 4;
	Thread->ArchContext.IrqStack = IntStack + KERNEL_INTERRUPT_STACK_SIZE * 3 / 4;
	Thread->ArchContext.FiqStack = IntStack + KERNEL_INTERRUPT_STACK_SIZE;
}

void KiSwitchArchSpecificContext(PKTHREAD Thread, PKTHREAD OldThread)
{
	if (OldThread) {
		KiSaveInterruptStacks(
			&OldThread->ArchContext.AbtStack,
			&OldThread->ArchContext.UndStack,
			&OldThread->ArchContext.IrqStack,
			&OldThread->ArchContext.FiqStack
		);
		
		KiSaveUserModeContext(
			&OldThread->ArchContext.UserLr,
			&OldThread->ArchContext.UserSp
		);
	}
	
	KiRestoreInterruptStacks(
		Thread->ArchContext.AbtStack,
		Thread->ArchContext.UndStack,
		Thread->ArchContext.IrqStack,
		Thread->ArchContext.FiqStack
	);
	
	KiRestoreUserModeContext(
		Thread->ArchContext.UserLr,
		Thread->ArchContext.UserSp
	);
}
