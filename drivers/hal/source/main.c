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
	
	while (true) {
		LogMsg("My thread's still running!!");
		for (int i = 0; i < 4; i++)
			ASM("hlt");
	}
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
