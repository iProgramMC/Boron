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
#include <ke/apc.h>
#include <ke/thread.h>

#define QUEUE_BIT(x) (1U << (x))
typedef uint32_t QUEUE_MASK;

enum
{
	// The idle priority class has just one spot.
	PRIORITY_IDLE = 0,
	
	// The low priority class has 7 spots.
	PRIORITY_LOW = 1,
	PRIORITY_LOW_MAX = 7,
	
	// The normal priority class has 12 spots.
	PRIORITY_NORMAL = 8,
	PRIORITY_NORMAL_MAX = 19,
	
	// The high priority class has 7 spots.
	PRIORITY_HIGH = 20,
	PRIORITY_HIGH_MAX = 26,
	
	// The realtime priority class has 5 spots.
	PRIORITY_REALTIME = 27,
	PRIORITY_REALTIME_MAX = 31,
	
	PRIORITY_COUNT = 32,
};

typedef struct _KSCHEDULER
{	
	// Execution queue mask. If a bit is set, that means that a thread
	// of the priority corresponding to that bit exists in the queue.
	QUEUE_MASK ExecQueueMask;
	
	LIST_ENTRY ExecQueue[PRIORITY_COUNT];
	
	PKTHREAD CurrentThread;
	PKTHREAD NextThread;
	
	PKTHREAD IdleThread;
	
	RBTREE TimerTree;
	
	void* IdleThreadStackTop;
	
	int ThreadsOnQueueCount;
	
	uint64_t TicksSpentNonIdle;
	
	// in ticks, copy of CurrentThread->QuantumUntil unless
	// CurrentThread is null, in which case it's zero
	uint64_t QuantumUntil;
}
KSCHEDULER, *PKSCHEDULER;

void KeSetPriorityThread(PKTHREAD Thread, int Priority);

void KeSchedulerInitUP();

void KeSchedulerInit();

NO_RETURN void KeSchedulerCommit();

void KeTimerTick();

#endif//BORON_KE_SCHED_H
