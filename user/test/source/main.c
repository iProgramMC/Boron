#include <boron.h>

extern int TestFunction(); // libtest.so

int _start()
{
	DbgPrint("Hello from test.exe!");
	
	int i = 0;
	while (true)
	{
		OSPrintf("Hello, world!  Number %d.\n", ++i);
		//OSSleep(1000);
	}
}
