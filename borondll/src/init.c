#include <boron.h>

void Test()
{
	__asm__("syscall");
}

void DLLEntryPoint()
{
	Test();
}
