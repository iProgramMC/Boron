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
#ifndef BORON_KE_THREAD_H
#define BORON_KE_THREAD_H

#include <arch.h>
#include <ex.h>

#include <ke/dpc.h>
#include <ke/thread.h>

typedef struct
{
	int Placeholder;
}
KSCHEDULER, *PKSCHEDULER;

#endif//BORON_KE_THREAD_H
