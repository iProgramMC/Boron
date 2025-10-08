#include <boron.h>
#include <string.h>

extern void RunTest1();
extern void RunTest2();
extern void RunTest3();
extern void RunTest4();
extern void RunTest5();
extern void RunTest6();

int _start()
{
	DbgPrint("Init is running!\n");
	
	//RunTest1();
	//RunTest2();
	//RunTest3();
	//RunTest4();
	//RunTest5();
	RunTest6();
	
	OSExitProcess(0);
}
