/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/scheds.h
	
Abstract:
	This header file contains the scheduler object and
	its manipulation functions.
	
Author:
	iProgramInCpp - 7 October 2023
***/
#ifndef BORON_KE_SCHED_H
#define BORON_KE_SCHED_H

#include <arch.h>
#include <ex.h>

#include <ke/dpc.h>
#include <ke/thread.h>

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
	
	LIST_ENTRY ExecQueue[PRIORITY_COUNT];
	
	PKTHREAD CurrentThread;
}
KSCHEDULER, *PKSCHEDULER;

PKSCHEDULER KeGetCurrentScheduler();

PKTHREAD KeGetCurrentThread();

void KeStartThread(PKTHREAD Thread);

void KeSchedulerInitUP();

void KiSchedulerInit();

NO_RETURN void KeSchedulerCommit();

#endif//BORON_KE_SCHED_H
