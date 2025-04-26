/***
	The Boron Operating System
	Copyright (C) 2023-2025 iProgramInCpp

Module name:
	main.c
	
Abstract:
	This module implements the main function of the test
	driver.
	
Author:
	iProgramInCpp - 20 October 2023
***/
#include "utils.h"
#include "tests.h"
#include <ps.h>
#include <string.h>

PKTHREAD CreateThread(PKTHREAD_START StartRoutine, void* Parameter)
{
	PETHREAD Thread = NULL;
	BSTATUS Status = PsCreateSystemThreadFast(&Thread, StartRoutine, Parameter, false);
	
	if (FAILED(Status))
		return NULL;
	
	return &Thread->Tcb;
}

void PerformDelay(int Ms, PKDPC Dpc)
{
	KTIMER Timer;
	
	KeInitializeTimer(&Timer);
	KeSetTimer(&Timer, Ms, Dpc);
	
	KeWaitForSingleObject(&Timer.Header, false, TIMEOUT_INFINITE, MODE_KERNEL);
}

void DumpHex(void* DataV, size_t DataSize, bool LogScreen)
{
	uint8_t* Data = DataV;
	
	#define A(x) (((x) >= 0x20 && (x) <= 0x7F) ? (x) : '.')
	
	const size_t PrintPerRow = 32;
	
	for (size_t i = 0; i < DataSize; i += PrintPerRow) {
		char Buffer[256];
		Buffer[0] = 0;
		
		//sprintf(Buffer + strlen(Buffer), "%04lx: ", i);
		sprintf(Buffer + strlen(Buffer), "%p: ", Data + i);
		
		for (size_t j = 0; j < PrintPerRow; j++) {
			if (i + j >= DataSize)
				strcat(Buffer, "   ");
			else
				sprintf(Buffer + strlen(Buffer), "%02x ", Data[i + j]);
		}
		
		strcat(Buffer, "  ");
		
		for (size_t j = 0; j < PrintPerRow; j++) {
			if (i + j >= DataSize)
				strcat(Buffer, " ");
			else
				sprintf(Buffer + strlen(Buffer), "%c", A(Data[i + j]));
		}
		
		if (LogScreen)
			LogMsg("%s", Buffer);
		else
			DbgPrint("%s", Buffer);
	}
}

NO_RETURN void DriverTestThread(UNUSED void* Parameter)
{
	//PerformProcessTest();
	//PerformMutexTest();
	//PerformBallTest();
	//PerformFireworksTest();
	//PerformHandleTest();
	//PerformApcTest();
	//PerformRwlockTest();
	//PerformObjectTest();
	//PerformMdlTest();
	//PerformIntTest();
	//PerformKeyboardTest();
	//PerformStorageTest();
	//PerformExObTest();
	//PerformCcbTest();
	//PerformMm1Test();
	//PerformMm2Test();
	PerformMm3Test();
	
	LogMsg(ANSI_GREEN "*** All tests have concluded." ANSI_RESET);
	KeTerminateThread(0);
}

BSTATUS DriverEntry(UNUSED PDRIVER_OBJECT Object)
{
	PKTHREAD Thread = CreateThread(DriverTestThread, NULL);
	
	if (!Thread)
		return STATUS_INSUFFICIENT_MEMORY;
	
	ObDereferenceObject(Thread);
	return STATUS_SUCCESS;
}
