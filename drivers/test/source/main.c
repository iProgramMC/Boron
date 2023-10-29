#include <main.h>
#include <ke.h>
#include <ex.h>

int* Test()
{
	static int stuff;
	return &stuff;
}

NO_RETURN void StartRoutine()
{
	LogMsg("Hello from test.sys's Start Routine!!");
	
	DbgSetProgressBarSteps(20);
	
	for (int i = 0; i < 20; i++) {
		DbgAddToProgressBar();
		LogMsg("My thread's still running!!  %d", i);
		for (int i = 0; i < 8; i++)
			ASM("hlt");
	}
	
	KeCrash("From test driver");
}

int DriverEntry()
{
	LogMsg("Hisssssssssssssssss, Viper Lives");
	
	// Create a basic thread.
	PKTHREAD Thread = KeCreateEmptyThread();
	
	KeInitializeThread(Thread, EX_NO_MEMORY_HANDLE, StartRoutine, NULL);
	
	KeSetPriorityThread(Thread, PRIORITY_REALTIME);
	
	KeReadyThread(Thread);
	
	*Test() = 69;
	
	return 42 + *Test();
}
