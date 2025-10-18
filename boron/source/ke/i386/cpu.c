/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/i386/cpu.c
	
Abstract:
	This module implements certain utility functions,
	as well as certain parts of UP initialization code,
	relating to CPU features such as the GDT and IDT.
	
Author:
	iProgramInCpp - 14 October 2025
***/
#include <main.h>
#include <arch.h>
#include <mm.h>
#include <string.h>
#include "../../ke/ki.h"

extern void KeOnUpdateIPL(KIPL newIPL, KIPL oldIPL); // defined in x.asm

static UNUSED uint32_t KepGetEflags()
{
	uint32_t eflags = 0;
	ASM("pushfd\n"
	    "popd %0":"=r"(eflags));
	return eflags;
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

#ifdef CONFIG_SMP // not supported but just in case you want it

void KeSetCPUPointer(void* pGS)
{
	KeSetMSR(MSR_GS_BASE, (uint64_t) pGS);
}

void* KeGetCPUPointer(void)
{
	return (void*) KeGetMSR(MSR_GS_BASE);
}

#else

static void* KiCPUPointer;

void KeSetCPUPointer(void* pGS)
{
	KiCPUPointer = pGS;
}

void* KeGetCPUPointer()
{
	return KiCPUPointer;
}

#endif


extern void* KiIdtDescriptor;

INIT
void KepLoadIdt()
{
	ASM("lidt (%0)"::"r"(&KiIdtDescriptor));
}

static uint64_t KepGdtEntries[] =
{
	0x0000000000000000, // Null descriptor
	0x00cf9b000000ffff, // 32-bit ring-0 code
	0x00cf93000000ffff, // 32-bit ring-0 data
	0x00cff3000000ffff, // 32-bit ring-3 data
	0x00cffb000000ffff, // 32-bit ring-3 code
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
	
	struct
	{
		uint16_t Length;
		uint64_t Pointer;
	}
	PACKED GdtDescriptor;
	
	GdtDescriptor.Length  = sizeof   * Gdt;
	GdtDescriptor.Pointer = (uint64_t) Gdt;
	
	void* Prcb = KeGetCPUPointer();
	KepLoadGdt(&GdtDescriptor);
	
	KeSetCPUPointer(Prcb);
	
	// also load the TSS
	KepLoadTss(offsetof(KGDT, TssEntry));
}

INIT
static void KepSetupTss(KTSS* Tss)
{
	// we'll set it up later..
	memset(Tss, 0, sizeof * Tss);
}

extern void KiSystemServiceHandler();

INIT
void KeInitCPU()
{
	KiSwitchToAddressSpaceProcess(KeGetSystemProcess());
	
	PKARCH_DATA Data = &KeGetCurrentPRCB()->ArchData;
	memset(&Data->Gdt, 0, sizeof Data->Gdt);
	
	KepSetupTss(&Data->Tss);
	KepSetupGdt(Data);
	KepLoadIdt();
/*
	// Set up the system call parameters now.
	// Enable the SYSCALL/SYSRET instructions and the NX bit.
	KeSetMSR(MSR_IA32_EFER, KeGetMSR(MSR_IA32_EFER) | MSR_IA32_EFER_SCE | 0);
	
	// Set the system call handler.
	KeSetMSR(MSR_IA32_LSTAR, (uint64_t) KiSystemServiceHandler);
	
	// Set the system call CS and SS.
	//
	// ARCHITECTURAL CRIMES AGAINST HUMANITY:
	// For the kernel CS/SS, you cannot pick SS.  It is automatically
	// picked as STAR_47_32 + 8.  But CS is the proper value of STAR_47_32.
	//
	// For the *user* CS/SS, you cannot pick SS.  It is automatically
	// picked as STAR_63_48 + 8.  But CS is NOT the proper value
	// of STAR_63_48, instead, it's STAR_63_48 + 16.  Why AMD?!
	//
	// (Or was it Intel?)
	uint64_t Star = SEG_RING_0_CODE | (((SEG_RING_3_DATA - 8) | 3) << 16);
	KeSetMSR(MSR_IA32_STAR, Star << 32);
	
	// Set the mask to disable interrupts on syscall.
	KeSetMSR(MSR_IA32_FMASK, 0x200);
*/
}

/*
extern uintptr_t KiSystemServiceTable[];
extern uintptr_t KiSystemServiceTableEnd[];

// If system call tracing is enabled, this shows all of the system calls happening.
void KePrintSystemServiceDebug(size_t Syscall)
{
	// Format: "[ThreadPointer] - Syscall [Number] ([FunctionName])"
	size_t Size = KiSystemServiceTableEnd - KiSystemServiceTable;
	
	const char* FunctionName;
	
	if (Syscall >= Size)
		FunctionName = "INVALID";
	else
		FunctionName = DbgLookUpRoutineNameByAddressExact(KiSystemServiceTable[Syscall]);
	
	DbgPrint("SYSCALL: %p - %d %s", KeGetCurrentThread(), (int) Syscall, FunctionName);
}
*/
