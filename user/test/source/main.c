#include <boron.h>

extern int TestFunction(); // libtest.so

int _start()
{
	DbgPrint("Hello from test.exe!");
	int x = 100;
	x += TestFunction();
	return x;
}
