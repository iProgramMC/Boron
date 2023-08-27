// Boron - Hardware abstraction
// CPU management
#include <main.h>
#include <hal.h>
#include <ke.h>
#include <mm.h>
#include <string.h>

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

HalArchData* HalGetData()
{
	return &KeGetCPU()->ArchData;
}

static void HalpSetupIdt(IDT* Idt)
{
	struct
	{
		uint16_t Length;
		uint64_t Pointer;
	}
	PACKED IdtDescriptor;
	
	IdtDescriptor.Length  = sizeof   * Idt;
	IdtDescriptor.Pointer = (uint64_t) Idt;
	
	ASM("lidt (%0)"::"r"(&IdtDescriptor));
}

static uint64_t HalpGdtEntries[] =
{
	0x0000000000000000, // Null descriptor
	0x00af9b000000ffff, // 64-bit ring-0 code
	0x00af93000000ffff, // 64-bit ring-0 data
	0x00affb000000ffff, // 64-bit ring-3 code
	0x00aff3000000ffff, // 64-bit ring-3 data
};

extern void HalpLoadGdt(void* desc);

static void HalpSetupGdt(HalArchData* Data)
{
	GDT* Gdt = &Data->Gdt;
	TSS* Tss = &Data->Tss;
	
	for (int i = 0; i < C_GDT_SEG_COUNT; i++)
	{
		Gdt->Segments[i] = HalpGdtEntries[i];
	}
	
	// setup the TSS entry:
	uintptr_t TssAddress = (uintptr_t) Tss;
	
	Gdt->TssEntry.Limit1 = sizeof(TSS);
	Gdt->TssEntry.Base1  = TssAddress;
	Gdt->TssEntry.Access = 0x9;
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
	
	HalpLoadGdt(&GdtDescriptor);
	
	// Also load the task register:
	//ASM("ltr %0"::"rm"((uint16_t) offsetof (GDT, TssEntry)));
}

static void HalpSetupTss(TSS* Tss)
{
	// we'll set it up later..
	memset(Tss, 0, sizeof * Tss);
}

void HalInitCPU()
{
	int ispPFN = MmAllocatePhysicalPage();
	int idtPFN = MmAllocatePhysicalPage();
	
	if (ispPFN == PFN_INVALID || idtPFN == PFN_INVALID)
	{
		// TODO: crash
		LogMsg("Error, can't initialize CPU %u, we don't have enough memory. Tried to create IDT & interrupt stack", KeGetCPU()->m_apicID);
		KeStopCurrentCPU();
	}
	
	HalArchData* Data = HalGetData();
	memset(&Data->Gdt, 0, sizeof Data->Gdt);
	
	IDT* idt = MmGetHHDMOffsetAddr(MmPFNToPhysPage(idtPFN));
	memset(idt, 0, sizeof * idt);
	
	void* intStack = MmGetHHDMOffsetAddr(MmPFNToPhysPage(ispPFN));
	Data->IntStack = intStack;
	memset(intStack, 0, 4096);
	
	HalpSetupTss(&Data->Tss);
	HalpSetupGdt(Data);
	HalpSetupIdt(idt);
	Data->Idt = idt;
}
