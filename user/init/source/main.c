#include <boron.h>
#include <string.h>

extern void RunTest1();
extern void RunTest2();
extern void RunTest3();

int _start()
{
	DbgPrint("Init is running!\n");
	
	//RunTest1();
	//RunTest2();
	RunTest3();
	
	OSExitThread();
}
