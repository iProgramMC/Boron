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
#include <ke/dispatch.h>

#define MAXIMUM_WAIT_BLOCKS (64)
#define THREAD_WAIT_BLOCKS (4)

#define KERNEL_STACK_SIZE (8192) // Note: Must be a multiple of PAGE_SIZE.

#define MAX_QUANTUM_US (10000)

typedef NO_RETURN void(*PKTHREAD_START)(void* Context);

typedef void(*PKTHREAD_TERMINATE_METHOD)(PKTHREAD Thread);

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
}
KTHREAD_STACK, *PKTHREAD_STACK;

struct KTHREAD_tag
{
	KDISPATCH_HEADER Header;
	
	LIST_ENTRY EntryGlobal;    // Entry into global thread list
	LIST_ENTRY EntryQueue;     // Entry into the CPU local dispatcher ready queue
	LIST_ENTRY EntryProc;      // Entry into the parent process' list of threads
	
	KPROCESSOR_MODE Mode;
	
	int ThreadId;
	
	KPRIORITY Priority; // base priority + priority boost
	KPRIORITY BasePriority;
	KPRIORITY PriorityBoost; // boosts for exactly one quantum
	
	bool Detached;
	
	PKPROCESS Process;
	
	int Status;
	
	int YieldReason;
	
	KTHREAD_STACK Stack;
	
	void* StackPointer; // Pass this into KiSwitchThreadStack.
	
	int WaitType;
	
	int WaitCount;
	
	KWAIT_BLOCK WaitBlocks[THREAD_WAIT_BLOCKS];
	
	PKWAIT_BLOCK WaitBlockArray;
	
	bool WaitIsAlertable;
	
	int WaitStatus;
	
	KPROCESSOR_MODE WaitMode;
	
	PKTHREAD_START StartRoutine;
	
	void* StartContext;
	
	uint64_t QuantumUntil;
	
	int LastProcessor;
	
	KAFFINITY Affinity;
	
	KDPC WaitDpc;
	
	KTIMER WaitTimer;
	
	KDPC DeleteDpc;
	
	// List of owned mutexes.
	LIST_ENTRY MutexList;
	
	PKPROCESS AttachedProcess;
	
	// Whether the thread is in MmProbeAddress.  This value is preserved
	// because we don't want to disable interrupts during the probe, but
	// we also don't want other threads' invalid page faults to jump to
	// the alternative exit.
	bool Probing;
	
	// Whether or not to "steal" this thread on another processor. Used
	// when exiting a wait.
	bool DontSteal;
	
	// APC queues.
	LIST_ENTRY UserApcQueue;
	LIST_ENTRY KernelApcQueue;
	
	// How many APCs are in progress right now.
	int ApcInProgress;
	
	int ApcDisableCount;
	
	// Priority increment after thread is terminated.
	KPRIORITY IncrementTerminated;
	
	// Method called when the thread terminates, in detached mode.
	// The method is called at IPL_DPC with the dispatcher database
	// locked.
	PKTHREAD_TERMINATE_METHOD TerminateMethod;
	
	// TODO: This doesn't belong in KTHREAD, but rather ETHREAD. This
	// layering violation will be fixed later.
	LIST_ENTRY DeferredIrpList;
};

// Creates an empty, uninitialized, thread object.
// TODO Use the object manager for this purpose and expose the thread object there.
PKTHREAD KeAllocateThread();

// Deletes a thread that was created by KeAllocateThread.
void KeDeallocateThread(PKTHREAD Thread);

// Reads the state of a thread.
int KeReadStateThread(PKTHREAD Thread);

// Detaches a child thread from the managing thread. This involves automatic cleanup
// through the terminate method specified in the respective function parameter.
//
// Notes:
// - If TerminateMethod is NULL, then the thread will use KeDeallocateThread as the
//   terminate method.  In that case, you must NOT use a pointer to a thread which
//   wasn't allocated using KeAllocateThread.  Thus, you should ideally discard the
//   pointer as soon as this function finishes, or as soon as you call KeReadyThread,
//   whichever comes last.
//
// - The thread pointer is only guaranteed to be accessible (with the rigorous dis-
//   patcher database locking of course) until `TerminateMethod' is called.
void KeDetachThread(PKTHREAD Thread, PKTHREAD_TERMINATE_METHOD TerminateMethod);

// Initializes the thread object.
NO_DISCARD BSTATUS KeInitializeThread(PKTHREAD Thread, void* KernelStack, PKTHREAD_START StartRoutine, void* StartContext, PKPROCESS Process);

// Readies the thread object for execution.
// Note. Don't call this more than once per thread! Bad things will happen!!
void KeReadyThread(PKTHREAD Thread);

// Yield this thread's time slice.
void KeYieldCurrentThread();

// Wakes up a thread after a wait.
void KeWakeUpThread(PKTHREAD Thread);

// Terminate the current thread.
NO_RETURN void KeTerminateThread(KPRIORITY Increment);

#endif//BORON_KE_THREAD_H
