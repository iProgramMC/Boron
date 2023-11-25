/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/thread.h
	
Abstract:
	This header file contains the thread object and
	its manipulation functions.
	
Author:
	iProgramInCpp - 28 September 2023
***/
#ifndef BORON_KE_THREAD_H
#define BORON_KE_THREAD_H

#include <rtl/list.h>
#include <arch.h>
#include <ex.h>
#include <ke/dispatch.h>

#define MAXIMUM_WAIT_BLOCKS (64)
#define THREAD_WAIT_BLOCKS (4)

#define KERNEL_STACK_SIZE (8192) // Note: Must be a multiple of PAGE_SIZE.

#define MAX_QUANTUM_US (10000)

typedef NO_RETURN void(*PKTHREAD_START)(void* Context);

typedef struct KTHREAD_tag KTHREAD, *PKTHREAD;

enum
{
	KTHREAD_STATUS_UNINITIALIZED, // Thread was not initialized.
	KTHREAD_STATUS_INITIALIZED,   // Thread was initialized, however, it has never run.
	KTHREAD_STATUS_READY,         // Thread is ready to be scheduled in but is currently not running.
	KTHREAD_STATUS_RUNNING,       // Thread is actively running.  This value should only be used on the current thread (KeGetCurrentThread()).
	KTHREAD_STATUS_WAITING,       // Thread is currently waiting for one or more objects. The WaitType member tells us how the wait is processed.
	KTHREAD_STATUS_TERMINATED,    // Thread was terminated and is awaiting cleanup.
};

typedef struct KTHREAD_STACK_tag
{
	void*  Top;
	size_t Size;
	EXMEMORY_HANDLE Handle;
}
KTHREAD_STACK, *PKTHREAD_STACK;

struct KTHREAD_tag
{
	KDISPATCH_HEADER Header;
	
	LIST_ENTRY EntryGlobal;    // Entry into global thread list
	LIST_ENTRY EntryList;      // Entry into CPU local scheduler's thread list
	LIST_ENTRY EntryQueue;     // Entry into the CPU local dispatcher ready queue
	LIST_ENTRY EntryProc;      // Entry into the parent process' list of threads
	
	int ThreadId;
	
	int Priority;
	
	bool Detached;
	
	PKPROCESS Process;
	
	int Status;
	
	int YieldReason;
	
	KTHREAD_STACK Stack;
	
	PKREGISTERS State; // Part of the kernel stack!!
	
	int WaitType;
	
	int WaitCount;
	
	KWAIT_BLOCK WaitBlocks[THREAD_WAIT_BLOCKS];
	
	PKWAIT_BLOCK WaitBlockArray;
	
	bool WaitIsAlertable;
	
	int WaitStatus;
	
	PKTHREAD_START StartRoutine;
	
	void* StartContext;
	
	uint64_t QuantumUntil;
	
	KAFFINITY Affinity;
	
	KDPC WaitDpc;
	
	KTIMER WaitTimer;
	
	KDPC DeleteDpc;
	
	LIST_ENTRY MutexList;
	
	PKPROCESS AttachedProcess;
	
	// Whether the thread is in MmProbeAddress.  This value is preserved
	// because we don't want to disable interrupts during the probe, but
	// we also don't want other threads' invalid page faults to jump to
	// the alternative exit.
	bool Probing;
};

// Creates an empty, uninitialized, thread object.
// TODO Use the object manager for this purpose and expose the thread object there.
PKTHREAD KeAllocateThread();

// Deletes a thread that was created by KeAllocateThread.
void KeDeallocateThread(PKTHREAD Thread);

// Reads the state of a thread.
int KeReadStateThread(PKTHREAD Thread);

// Detaches a thread. This involves automatic cleanup through KeDeallocateThread.
// Don't call if the thread instance wasn't allocated with KeAllocateThread.
// After the call, treat Thread as an invalid pointer and throw it away.
void KeDetachThread(PKTHREAD Thread);

// Initializes the thread object.
void KeInitializeThread(PKTHREAD Thread, EXMEMORY_HANDLE KernelStack, PKTHREAD_START StartRoutine, void* StartContext, PKPROCESS Process);

// Readies the thread object for execution.
// Note. Don't call this more than once per thread! Bad things will happen!!
void KeReadyThread(PKTHREAD Thread);

// Yield this thread's time slice.
void KeYieldCurrentThread();

// Wakes up a thread after a wait.
void KeWakeUpThread(PKTHREAD Thread);

// Terminate the current thread.
NO_RETURN void KeTerminateThread();

#endif//BORON_KE_THREAD_H
