#include <boron.h>
#include <string.h>

#define CRASH(...) do {    \
	DbgPrint("CRASH:");    \
	DbgPrint(__VA_ARGS__); \
	OSExitThread();        \
} while (0)

extern void RunTest1();

int _start()
{
	DbgPrint("Init is running!\n");
	
	RunTest1();
	
	OSExitThread();
}
