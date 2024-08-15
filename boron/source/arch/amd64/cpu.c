/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	arch/amd64/cpu.c
	
Abstract:
	This module implements certain utility functions,
	as well as certain parts of UP initialization code,
	relating to CPU features such as the GDT and IDT.
	
Author:
	iProgramInCpp - 20 August 2023
***/
#include <main.h>
#include <arch.h>
#include <mm.h>
#include <string.h>
#include "../../ke/ki.h"

void KeWaitForNextInterrupt()
{
	ASM("hlt":::"memory");
}

void KeInvalidatePage(void* page)
{
	ASM("invlpg (%0)"::"r"((uintptr_t)page):"memory");
}

extern void KeOnUpdateIPL(KIPL newIPL, KIPL oldIPL); // defined in x.asm

static UNUSED uint64_t KepGetRflags()
{
	uint64_t rflags = 0;
	ASM("pushfq\n"
	    "popq %0":"=r"(rflags));
	return rflags;
}

void KeSpinningHint()
{
	ASM("pause":::"memory");
}

// Model specific registers

uint64_t KeGetMSR(uint32_t msr)
{
	uint32_t edx, eax;
	
	ASM("rdmsr":"=d"(edx),"=a"(eax):"c"(msr));
	
	return ((uint64_t)edx << 32) | eax;
}

void KeSetMSR(uint32_t msr, uint64_t value)
{
	uint32_t edx = (uint32_t)(value >> 32);
	uint32_t eax = (uint32_t)(value);
	
	ASM("wrmsr"::"d"(edx),"a"(eax),"c"(msr));
}

void KeSetCPUPointer(void* pGS)
{
	KeSetMSR(MSR_GS_BASE_KERNEL, (uint64_t) pGS);
}

void* KeGetCPUPointer(void)
{
	return (void*) KeGetMSR(MSR_GS_BASE_KERNEL);
}

KARCH_DATA* KeGetData()
{
	return &KeGetCurrentPRCB()->ArchData;
}

extern void* KiIdtDescriptor;

INIT
void KepLoadIdt()
{
	ASM("lidt (%0)"::"r"(&KiIdtDescriptor));
}

static uint64_t KepGdtEntries[] =
{
	0x0000000000000000, // Null descriptor
	0x00af9b000000ffff, // 64-bit ring-0 code
	0x00af93000000ffff, // 64-bit ring-0 data
	0x00affb000000ffff, // 64-bit ring-3 code
	0x00aff3000000ffff, // 64-bit ring-3 data
};

extern void KepLoadGdt(void* desc);
extern void KepLoadTss(int descriptor);

INIT
static void KepSetupGdt(KARCH_DATA* Data)
{
	KGDT* Gdt = &Data->Gdt;
	KTSS* Tss = &Data->Tss;
	
	for (int i = 0; i < C_GDT_SEG_COUNT; i++)
	{
		Gdt->Segments[i] = KepGdtEntries[i];
	}
	
	// setup the TSS entry:
	uintptr_t TssAddress = (uintptr_t) Tss;
	
	Gdt->TssEntry.Limit1 = sizeof(KTSS);
	Gdt->TssEntry.Base1  = TssAddress;
	Gdt->TssEntry.Access = 0x89;
	Gdt->TssEntry.Limit2 = 0x0;
	Gdt->TssEntry.Flags  = 0x0;
	Gdt->TssEntry.Base2  = TssAddress >> 24;
	Gdt->TssEntry.Resvd  = 0;
	
	struct
	{
		uint16_t Length;
		uint64_t Pointer;
	}
	PACKED GdtDescriptor;
	
	GdtDescriptor.Length  = sizeof   * Gdt;
	GdtDescriptor.Pointer = (uint64_t) Gdt;
	
	KepLoadGdt(&GdtDescriptor);
	
	// also load the TSS
	KepLoadTss(offsetof(KGDT, TssEntry));
}

INIT
static void KepSetupTss(KTSS* Tss)
{
	// we'll set it up later..
	memset(Tss, 0, sizeof * Tss);
}

INIT
void KeInitCPU()
{
	KiSwitchToAddressSpaceProcess(KeGetSystemProcess());
	
	int ispPFN = MmAllocatePhysicalPage();
	
	if (ispPFN == PFN_INVALID)
	{
		// TODO: crash
		LogMsg("Error, can't initialize CPU %u, we don't have enough memory. Tried to create interrupt stack", KeGetCurrentPRCB()->LapicId);
		KeStopCurrentCPU();
	}
	
	KARCH_DATA* Data = KeGetData();
	memset(&Data->Gdt, 0, sizeof Data->Gdt);
	
	void* intStack = MmGetHHDMOffsetAddr(MmPFNToPhysPage(ispPFN));
	Data->IntStack = intStack;
	memset(intStack, 0, 4096);
	
	KepSetupTss(&Data->Tss);
	KepSetupGdt(Data);
	KepLoadIdt();
}

#ifdef DEBUG

void DbgPrintString(const char* str)
{
	while (*str)
	{
		KePortWriteByte(0xE9, *str);
		str++;
	}
}

#endif
