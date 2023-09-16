// Boron - Hardware abstraction
// CPU management
#include <main.h>
#include <arch.h>
#include <hal.h>
#include <ke.h>
#include <mm.h>
#include <string.h>

void KeWaitForNextInterrupt()
{
	ASM("hlt":::"memory");
}

void KeInvalidatePage(void* page)
{
	ASM("invlpg (%0)"::"r"((uintptr_t)page):"memory");
}

extern void KeOnUpdateIPL(eIPL newIPL, eIPL oldIPL); // defined in x.asm

void KeSetInterruptsEnabled(bool b)
{
	if (b)
		ASM("sti":::"memory");
	else
		ASM("cli":::"memory");
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

KeArchData* KeGetData()
{
	return &KeGetCPU()->ArchData;
}

extern void* KiIdtDescriptor;
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

static void KepSetupGdt(KeArchData* Data)
{
	GDT* Gdt = &Data->Gdt;
	TSS* Tss = &Data->Tss;
	
	for (int i = 0; i < C_GDT_SEG_COUNT; i++)
	{
		Gdt->Segments[i] = KepGdtEntries[i];
	}
	
	// setup the TSS entry:
	uintptr_t TssAddress = (uintptr_t) Tss;
	
	Gdt->TssEntry.Limit1 = sizeof(TSS);
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
	KepLoadTss(offsetof(GDT, TssEntry));
}

static void KepSetupTss(TSS* Tss)
{
	// we'll set it up later..
	memset(Tss, 0, sizeof * Tss);
}

void KeInitCPU()
{
	// create a new page mapping based on the one that already exists:
	uintptr_t Map = MmCreatePageMapping(KeGetCurrentPageTable());
	if (Map == 0)
	{
		LogMsg("Error, mapping is zero");
		KeStopCurrentCPU();
	}
	
	LogMsg("Map for CPU %u is %x", KeGetCPU()->LapicId, Map);
	
	KeSetCurrentPageTable(Map);
	
	int ispPFN = MmAllocatePhysicalPage();
	
	if (ispPFN == PFN_INVALID)
	{
		// TODO: crash
		LogMsg("Error, can't initialize CPU %u, we don't have enough memory. Tried to create interrupt stack", KeGetCPU()->LapicId);
		KeStopCurrentCPU();
	}
	
	KeArchData* Data = KeGetData();
	memset(&Data->Gdt, 0, sizeof Data->Gdt);
	
	void* intStack = MmGetHHDMOffsetAddr(MmPFNToPhysPage(ispPFN));
	Data->IntStack = intStack;
	memset(intStack, 0, 4096);
	
	KepSetupTss(&Data->Tss);
	KepSetupGdt(Data);
	KepLoadIdt();
	
	#ifdef TARGET_AMD64
	HalEnableApic();
	#endif
}
