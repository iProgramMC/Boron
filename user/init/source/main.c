#include <boron.h>
#include <string.h>

extern void RunTest1();
extern void RunTest2();

int _start()
{
	DbgPrint("Init is running!\n");
	
	//RunTest1();
	RunTest2();
	
	OSExitThread();
}
