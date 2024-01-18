/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/sched.h
	
Abstract:
	This header file contains the scheduler object and
	its manipulation functions.
	
Author:
	iProgramInCpp - 7 October 2023
***/
#ifndef BORON_KE_SCHED_H
#define BORON_KE_SCHED_H

#include <arch.h>

#include <ke/dpc.h>
#include <ke/thread.h>

#define QUEUE_BIT(x) (1U << (x))
typedef uint32_t QUEUE_MASK;

enum
{
	PRIORITY_IDLE,
	PRIORITY_BGWORKER,
	PRIORITY_XLOW,
	PRIORITY_LOW,
	PRIORITY_NORMAL,
	PRIORITY_HIGH,
	PRIORITY_XHIGH,
	PRIORITY_REALTIME,
	PRIORITY_COUNT,
};

typedef struct
{
	LIST_ENTRY ThreadList;
	
	// Execution queue mask. If a bit is set, that means that a thread
	// of the priority corresponding to that bit exists in the queue.
	QUEUE_MASK ExecQueueMask;
	
	LIST_ENTRY ExecQueue[PRIORITY_COUNT];
	
	PKTHREAD CurrentThread;
	PKTHREAD NextThread;
	
	PKTHREAD IdleThread;
	
	AVLTREE TimerTree;
	
	BIG_MEMORY_HANDLE IdleThreadStack;
	
	// in ticks, copy of CurrentThread->QuantumUntil unless
	// CurrentThread is null, in which case it's zero
	uint64_t QuantumUntil;
}
KSCHEDULER, *PKSCHEDULER;

PKSCHEDULER KeGetCurrentScheduler();

PKTHREAD KeGetCurrentThread();

void KeSetPriorityThread(PKTHREAD Thread, int Priority);

void KeSchedulerInitUP();

void KeSchedulerInit();

NO_RETURN void KeSchedulerCommit();

void KeTimerTick();

#endif//BORON_KE_SCHED_H
