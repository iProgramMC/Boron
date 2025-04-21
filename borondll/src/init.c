#include <boron.h>

void Test()
{
	__asm__("syscall");
}

void Test2()
{
	__asm__("nop");
	__asm__("nop");
	__asm__("nop");
	__asm__("syscall");
}

void DLLEntryPoint()
{
	Test();
	Test2();
}
