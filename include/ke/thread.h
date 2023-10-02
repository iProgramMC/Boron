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

#include <arch.h>
#include <ex.h>

typedef struct KTHREAD_tag
{
	KREGISTERS State;
	
}
KTHREAD, *PKTHREAD;

typedef struct
{
	int Placeholder;
}
KSCHEDULER, *PKSCHEDULER;

PKTHREAD KeCreateThread();

#endif//BORON_KE_THREAD_H
