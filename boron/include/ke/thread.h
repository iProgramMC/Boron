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

#define MAX_QUANTUM_US (10000)

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
	
	bool Suspended;
	
	// Whether this thread is a user thread controlled by Ps.
	bool IsUserThread;
	
	// Whether the thread is in MmProbeAddress.  This value is preserved
	// because we don't want to disable interrupts during the probe, but
	// we also don't want other threads' invalid page faults to jump to
	// the alternative exit.
	bool Probing;
	
	// Whether or not to "steal" this thread on another processor. Used
	// when exiting a wait.
	bool DontSteal;
	
	// Whether the thread is marked terminated.  Before a thread descends
	// into user mode, this value is checked, and the thread will exit.
	bool PendingTermination;
	
	// APC queues.
	LIST_ENTRY UserApcQueue;
	LIST_ENTRY KernelApcQueue;
	
	// How many APCs are in progress right now.
	int ApcInProgress;
	
	int ApcDisableCount;
	
	// The tick when this thread was scheduled.
	uint64_t TickScheduledAt;
	
	// Priority increment after thread is terminated.
	KPRIORITY IncrementTerminated;
	
	// Method called when the thread terminates.
	// The method is called in a DPC context.
	PKTHREAD_TERMINATE_METHOD TerminateMethod;
	
	// The timer used for the OSSleep system call.
	KTIMER SleepTimer;
	
	// Architecture-specific context.
	KTHREAD_ARCH_CONTEXT ArchContext;
	
#ifdef DEBUG
	// How many spin locks a thread is holding.
	// This is used when initiating a wait at high IPL
	// to ensure the thread doesn't hold any locks.
	int HoldingSpinlocks;
	
	uint64_t EnqueuedTime;
#endif
	
	// When calling KeReleaseMutexWait, KeReleaseSemaphoreWait,
	// KeSetEventWait, KeResetEventWait, or KePulseEventWait,
	// these members are used to restore the IPL to the original
	// IPL.
	bool DidCallWaitFunction;
	
	KIPL OriginalIplWait;
	
	// User-space pointer to the TEB (thread environment block).
	void* TebPointer;
};

// Creates an empty, uninitialized, thread object.
// TODO Use the object manager for this purpose and expose the thread object there.
PKTHREAD KeAllocateThread();

// Deletes a thread that was created by KeAllocateThread.
void KeDeallocateThread(PKTHREAD Thread);

// Reads the state of a thread.
int KeReadStateThread(PKTHREAD Thread);

void KeSetTerminateMethodThread(PKTHREAD Thread, PKTHREAD_TERMINATE_METHOD TerminateMethod);

// Initializes the thread object.
void KeInitializeThread(PKTHREAD Thread, void* KernelStack, PKTHREAD_START StartRoutine, void* StartContext, PKPROCESS Process);
void KeInitializeThread2(PKTHREAD Thread, void* KernelStack, size_t KernelStackSize, PKTHREAD_START StartRoutine, void* StartContext, PKPROCESS Process);

// Readies the thread object for execution.
// Note. Don't call this more than once per thread! Bad things will happen!!
void KeReadyThread(PKTHREAD Thread);

// Yield this thread's time slice.
void KeYieldCurrentThread();

// Terminate the current thread.
NO_RETURN void KeTerminateThread(KPRIORITY Increment);

// Terminate another thread.
void KeTerminateThread2(PKTHREAD Thread, KPRIORITY Increment);

// Sets a thread's suspended status.
void KeSetSuspendedThread(PKTHREAD Thread, bool IsSuspended);

// Switch this thread into user mode.
NO_RETURN void KeDescendIntoUserMode(void* InstructionPointer, void* StackPointer, void* UserContext);

// Gets the current userspace TEB pointer.  This directly matches up with usermode's FS base on x86 and amd64.
void* OSGetCurrentTeb();

// Gets the current userspace PEB pointer.  This directly matches up with usermode's GS base on x86 and amd64.
void* OSGetCurrentPeb();

// Sets the current userspace TEB pointer.  The kernel will not attempt to use this pointer.
// This also assigns the FS base for user mode on x86 and amd64.
BSTATUS OSSetCurrentTeb(void* Ptr);

// Sets the current userspace PEB pointer.  The kernel will not attempt to use this pointer.
// This also assigns the GS base for user mode on x86 and amd64.
BSTATUS OSSetCurrentPeb(void* Ptr);

#endif//BORON_KE_THREAD_H
