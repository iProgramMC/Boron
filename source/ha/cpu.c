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

extern void HalOnUpdateIPL(eIPL oldIPL, eIPL newIPL); // defined in x.asm

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

// Model specific registers

uint64_t HalGetMSR(uint32_t msr)
{
	uint32_t edx, eax;
	
	ASM("rdmsr":"=d"(edx),"=a"(eax):"c"(msr));
	
	return ((uint64_t)edx << 32) | eax;
}

void HalSetMSR(uint32_t msr, uint64_t value)
{
	uint32_t edx = (uint32_t)(value >> 32);
	uint32_t eax = (uint32_t)(value);
	
	ASM("wrmsr"::"d"(edx),"a"(eax),"c"(msr));
}

void HalSetCPUPointer(void* pGS)
{
	HalSetMSR(MSR_GS_BASE_KERNEL, (uint64_t) pGS);
}

void* HalGetCPUPointer(void)
{
	return (void*) HalGetMSR(MSR_GS_BASE_KERNEL);
}