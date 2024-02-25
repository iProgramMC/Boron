/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ke/apc.h
	
Abstract:
	This module contains the definitions related to the
	asynchronous procedure call system.
	
Author:
	iProgramInCpp - 21 February 2024
***/
#pragma once

typedef struct _KAPC KAPC, *PKAPC;
typedef struct KTHREAD_tag KTHREAD, *PKTHREAD;

// Pointer to routine to be executed in the specified context.
typedef void(*PKAPC_NORMAL_ROUTINE)(void* Context, void* SystemArgument1, void* SystemArgument2);

// Pointer to routine to be executed in kernel mode, at IPL_APC.
typedef void(*PKAPC_KERNEL_ROUTINE)(
	PKAPC Apc,
	PKAPC_NORMAL_ROUTINE* InOutNormalRoutine,
	void** InOutNormalContext,
	void** InOutSystemArgument1,
	void** InOutSystemArgument2
);

struct _KAPC
{
	// The thread the APC belongs to.
	PKTHREAD Thread;
	
	// A special APC can break into thread execution any time the thread is running
	// at normal IPL. Normal APCs can only break into thread execution while it is
	// running at normal IPL, and there isn't a normal APC currently running.
	//
	// During execution of special APCs, kernel services are available, but not all
	// system services may be available.
	//
	// A special APC is a kernel-mode APC that lacks a normal routine.
	
	// Routine to be executed in kernel mode at IPL APC.
	PKAPC_KERNEL_ROUTINE KernelRoutine;
	
	// Routine to be executed in the specified APC mode (kernel or user), as well
	// as parameters. They are set by KeInitializeApc, and can be modified by the
	// KernelRoutine.
	PKAPC_NORMAL_ROUTINE NormalRoutine;
	void* NormalContext;
	void* SystemArgument1;
	void* SystemArgument2;
	
	KPROCESSOR_MODE ApcMode;
	
	// Entry into the thread's list of APCs.
	LIST_ENTRY ListEntry;
	
	// Whether the APC is enqueued.
	bool Enqueued;
};

void KeInitializeApc(
	PKAPC Apc,
	PKTHREAD Thread,
	PKAPC_KERNEL_ROUTINE KernelRoutine,
	PKAPC_NORMAL_ROUTINE NormalRoutine,
	void* NormalContext,
	KPROCESSOR_MODE ApcMode
);

bool KeInsertQueueApc(
	PKAPC Apc,
	void* SystemArgument1,
	void* SystemArgument2,
	KPRIORITY Increment
);
