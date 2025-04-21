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
	
	// privileged instruction that's supposed to crash HERE
	__asm__("hlt");
	
	// invalid opcode for fun
	__asm__("ud2");
}
