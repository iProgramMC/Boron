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

#define MAXIMUM_WAIT_OBJECTS (64)

#define KERNEL_STACK_SIZE (8192) // Note: Must be a multiple of PAGE_SIZE.

typedef NO_RETURN void(*PKSTART_ROUTINE)(void* Context);

enum
{
	KTHREAD_STATUS_UNINITIALIZED, // Thread was not initialized.
	KTHREAD_STATUS_INITIALIZED,   // Thread was initialized, however, it has never run.
	KTHREAD_STATUS_READY,         // Thread is ready to be scheduled in but is currently not running.
	KTHREAD_STATUS_RUNNING,       // Thread is actively running.  This value should only be used on the current thread (KeGetCurrentThread()).
	KTHREAD_STATUS_WAITING,       // Thread is currently waiting for one or more objects. The WaitType member tells us how the wait is processed.
	KTHREAD_STATUS_TERMINATED,    // Thread was terminated and is awaiting cleanup.
};

enum
{
	KTHREAD_WAIT_ANY, // If any one of the wait objects is signalled, the entire thread is waken up
	KTHREAD_WAIT_ALL, // The thread is only waken up if ALL the wait objects 
};

enum
{
	KTHREAD_YIELD_QUANTUM, // Yielded because quantum expired.
	KTHREAD_YIELD_WAITING, // Yielded because thread is waiting on a dispatcher object.
};

enum
{
	KWAIT_BLOCK_RESOLVED, // If resolved, the wait block doesn't represent an actual entry into a linked list of entries anymore.
	KWAIT_BLOCK_WAITING,
};

typedef struct KWAIT_BLOCK_tag
{
	LIST_ENTRY Entry;      // Entry into the linked list of threads waiting for an object.
	void*      Object;     // The object that the containing thread waits on.
	uint8_t    WaitNumber; // The wait number. This is used to get the KTHREAD instance from a wait block instance.
	                       // It represents the index into the WaitBlock array.
	uint8_t    Status;     // The wait block's status.
}
KWAIT_BLOCK, PKWAIT_BLOCK;

typedef struct KTHREAD_STACK_tag
{
	void*  Top;
	size_t Size;
	EXMEMORY_HANDLE Handle;
}
KTHREAD_STACK, *PKTHREAD_STACK;

typedef struct KTHREAD_tag
{
	LIST_ENTRY EntryGlobal;    // Entry into global thread list
	LIST_ENTRY EntryList;      // Entry into CPU local scheduler's thread list
	LIST_ENTRY EntryQueue;     // Entry into the CPU local dispatcher ready queue
	LIST_ENTRY EntryProc;      // Entry into the parent process' list of threads
	
	int ThreadId;
	
	int Priority;
	
	// TODO Parent Process Pointer
	
	int Status;
	
	int YieldReason;
	
	KTHREAD_STACK Stack;
	
	KREGISTERS Registers;
	
	int WaitType;
	
	KWAIT_BLOCK WaitBlock[MAXIMUM_WAIT_OBJECTS];
	
	PKSTART_ROUTINE StartRoutine;
	
	void* StartContext;
}
KTHREAD, *PKTHREAD;

// Creates an empty, uninitialized, thread object.
// TODO Use the object manager for this purpose and expose the thread object there.
PKTHREAD KeCreateEmptyThread();

// Set the start function of the thread.
void KeSetStartFunctionThread(PKTHREAD Thread, PKSTART_ROUTINE StartRoutine, void* StartContext);

// Initialize the default stacks.
void KeInitializeStackDefault(PKTHREAD Thread);

// Assign an executive memory handle as a stack for a thread.
void KeSetStackThread(PKTHREAD Thread, EXMEMORY_HANDLE Memory);

// Allocate a stack for a thread using the default method of
// calling KiAllocateDefaultStack().
void KeUseDefaultStackThread(PKTHREAD Thread);

// Initializes the thread object. Must have called the setup functions
// KeSetStackThread (by itself or through KeUseDefaultStackThread), and
// KeSetStartFunctionThread.
void KeInitializeThread(PKTHREAD Thread);

#endif//BORON_KE_THREAD_H
