/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ps/psp.h
	
Abstract:
	This header defines private functions for Boron's
	process manager.
	
Author:
	iProgramInCpp - 30 August 2024
***/
#pragma once

#include <ke.h>
#include <ps.h>
#include <ex.h>
#include <string.h>

#define USER_STACK_SIZE (256 * 1024)

// Initial Virtual Address Range
#define INITIAL_BEG_VA MM_FIRST_USER_PAGE
#define INITIAL_END_VA MM_LAST_USER_PAGE

typedef struct
{
	void* InstructionPointer;
	void* UserContext;
}
THREAD_START_CONTEXT, *PTHREAD_START_CONTEXT;

bool PsCreateThreadType();

bool PsCreateProcessType();

NO_RETURN void PspUserThreadStart(void* Context);

void PsInitShutDownWorkerThread();

// Process List
void PspInitializeProcessList();

BSTATUS PspAddProcessToList(PEPROCESS Process);

void PspRemoveProcessFromList(PEPROCESS Process);
