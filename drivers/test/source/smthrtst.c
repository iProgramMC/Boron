/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	smthrtst.c
	
Abstract:
	This module implements the simple thread test.
	
	It creates two threads that together never idle.
	
Author:
	iProgramInCpp - 18 March 2026
***/
#include <ke.h>
#include <hal.h>
#include <string.h>
#include "utils.h"

NO_RETURN
void SttThread1(UNUSED void* Context)
{
	long long i = 1;
	while (true)
	{
		uint64_t ctt = HalGetTickCount();
		uint64_t cttP1S = ctt + HalGetTickFrequency();
		
		while (HalGetTickCount() < cttP1S) {
			i++;
		}
		
		LogMsg("Thread 1 came up with %lld.", i);
	}
}

NO_RETURN
void SttThread2(UNUSED void* Context)
{
	long long i = 1;
	while (true)
	{
		uint64_t ctt = HalGetTickCount();
		uint64_t cttP1S = ctt + HalGetTickFrequency();
		
		while (HalGetTickCount() < cttP1S) {
			i--;
		}
		
		LogMsg("Thread 2 came up with %lld.", i);
	}
}

void PerformTwoThreadsTest()
{
	PKTHREAD Thrd1 = CreateThread(SttThread1, NULL);
	PKTHREAD Thrd2 = CreateThread(SttThread2, NULL);
	
	void* Objects[2] = { Thrd1, Thrd2 };
	KeWaitForMultipleObjects(2, Objects, WAIT_TYPE_ALL, false, TIMEOUT_INFINITE, NULL, MODE_KERNEL);
	
	DbgPrint("PerformTwoThreadsTest done");
}