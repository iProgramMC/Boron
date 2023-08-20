// Boron - Hardware abstraction
// CPU management
#include <main.h>
#include <hal.h>

void HalWaitForNextInterrupt()
{
	ASM("hlt":::"memory");
}

void HalInvalidatePage(void* page)
{
	ASM("invlpg (%0)"::"r"((uintptr_t)page):"memory");
}

void HalSetInterruptsEnabled(bool b)
{
	if (b)
		ASM("sti":::"memory");
	else
		ASM("cli":::"memory");
}

void HalInterruptHint()
{
	ASM("pause":::"memory");
}
