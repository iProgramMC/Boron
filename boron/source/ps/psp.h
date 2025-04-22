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
#define INITIAL_BEG_VA 0x0000000000001000
#define INITIAL_END_VA 0x00007FFFFFFFF000

typedef struct
{
	void* InstructionPointer;
	void* UserContext;
}
THREAD_START_CONTEXT, *PTHREAD_START_CONTEXT;

bool PsCreateThreadType();

bool PsCreateProcessType();

NO_RETURN void PspUserThreadStart(void* Context);
