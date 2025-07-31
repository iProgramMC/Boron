#include <boron.h>
#include <string.h>
#include "misc.h"

NO_RETURN
void Work1()
{
	while (true)
	{
		DbgPrint("Work1 thread doing some work ...");
		OSSleep(500);
	}
}

NO_RETURN
void Work2()
{
	while (true)
	{
		DbgPrint("Work2 thread doing some work ...");
		OSSleep(500);
	}
}

NO_RETURN
void Work3()
{
	while (true)
	{
		DbgPrint("Work3 thread doing some work ...");
		OSSleep(500);
	}
}

void RunTest3()
{
	DbgPrint("Creating threads ...");
	
	HANDLE Thread1, Thread2, Thread3;
	BSTATUS Status;
	
	Status = OSCreateThread(&Thread1, CURRENT_PROCESS_HANDLE, NULL, Work1, NULL, false);
	CHECK_STATUS(3);
	
	Status = OSCreateThread(&Thread2, CURRENT_PROCESS_HANDLE, NULL, Work2, NULL, false);
	CHECK_STATUS(3);
	
	Status = OSCreateThread(&Thread3, CURRENT_PROCESS_HANDLE, NULL, Work3, NULL, false);
	CHECK_STATUS(3);
	
	OSClose(Thread1);
	OSClose(Thread2);
	OSClose(Thread3);
	
	DbgPrint("Letting the threads run wild for a bit ...");
	OSSleep(1500);
	
	DbgPrint("Now forcefully declaring the job done.");
	OSExitProcess();
}
