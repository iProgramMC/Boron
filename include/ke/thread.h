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

enum
{
	KTHREAD_STATUS_SUSPENDED,  // Thread is suspended. This could be because it's initializing.
	KTHREAD_STATUS_RUNNING,    // Thread is active running.  This value should only be used on the current thread (KeGetCurrentThread()).
	KTHREAD_STATUS_WAITING,    // Thread is currently waiting for one or more objects. The WaitType member tells us how the wait is processed.
};

enum
{
	KTHREAD_WAIT_ANY, // If any one of the wait objects is signalled, the entire thread is waken up
	KTHREAD_WAIT_ALL, // The thread is only waken up if ALL the wait objects 
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


typedef struct KTHREAD_tag
{
	LIST_ENTRY Entry;
	
	int ThreadId;
	
	// Process Pointer
	
	int Status;
	
	KREGISTERS Registers;
	
	int WaitType;
	
	KWAIT_BLOCK WaitBlock[MAXIMUM_WAIT_OBJECTS];
	
}
KTHREAD, *PKTHREAD;

PKTHREAD KeCreateThread();

#endif//BORON_KE_THREAD_H
