#include <main.h>
#include <ke.h>
#include <ex.h>

int* Test()
{
	static int stuff;
	return &stuff;
}

void KeYieldCurrentThread();

NO_RETURN void Start1Routine()
{
	LogMsg("Hello from test.sys's Start 1 Routine!!");
	
	for (int i = 0; ; i++) {
		LogMsg("\x1B[15;1HMy first thread's still running!!  %d", i);
		KeYieldCurrentThread();
	}
	
	KeCrash("From test driver");
}

NO_RETURN void Start2Routine()
{
	LogMsg("Hello from test.sys's Start 1 Routine!!");
	
	for (int i = 0; ; i++) {
		LogMsg("\x1B[20;1HMy second thread's still running!!  %d", i);
		KeYieldCurrentThread();
	}
}

int DriverEntry()
{
	LogMsg("Hisssssssssssssssss, Viper Lives");
	
	// Create a basic thread.
	PKTHREAD Thread1 = KeCreateEmptyThread();
	KeInitializeThread(Thread1, EX_NO_MEMORY_HANDLE, Start1Routine, NULL);
	KeSetPriorityThread(Thread1, PRIORITY_REALTIME);
	
	PKTHREAD Thread2 = KeCreateEmptyThread();
	KeInitializeThread(Thread2, EX_NO_MEMORY_HANDLE, Start2Routine, NULL);
	KeSetPriorityThread(Thread2, PRIORITY_REALTIME);
	
	KeReadyThread(Thread1);
	KeReadyThread(Thread2);
	
	*Test() = 69;
	
	return 42 + *Test();
}
