// Boron - Hardware abstraction
// CPU management
#include <main.h>
#include <arch.h>
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

extern void KeOnUpdateIPL(eIPL oldIPL, eIPL newIPL); // defined in x.asm

void KeSetInterruptsEnabled(bool b)
{
	if (b)
		ASM("sti":::"memory");
	else
		ASM("cli":::"memory");
}

void KeInterruptHint()
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

// This loads the bare interrupt vector handler into the IDT. It does not handle CPU independent IRQ handlers.
typedef void(*KepInterruptVector)();

// Probably not going to be used because SYSCALL and SYSENTER both bypass the ring system.
// These are going to be optimized out, and are also niceties when needed, so I will keep them.
static UNUSED void KepSetInterruptDPL(IDT* Idt, int Vector, int Ring)
{
	Idt->Entries[Vector].DPL = Ring;
}

// This is also not likely to be used. We need interrupts to be disabled automatically
// before we raise the IPL to the interrupt's level, but trap gates will not disable interrupts.
// This is bad, since a keyboard interrupt could manage to sneak past an important clock interrupt
// before we manage to raise the IPL in the clock interrupt.
// Not to mention the performance gains would be minimal if at all existent.
static UNUSED void KepSetInterruptGateType(IDT* Idt, int Vector, int GateType)
{
	Idt->Entries[Vector].GateType = GateType;
}

// Set the IST of an interrupt vector. This is probably useful for the double fault handler,
// as it is triggered only when another interrupt failed to be called, which is useful in cases
// where the kernel stack went missing and bad (Which I hope there aren't any!)
static UNUSED void KepSetInterruptStackIndex(IDT* Idt, int Vector, int Ist)
{
	Idt->Entries[Vector].IST = Ist;
}

// Parameters:
// Idt     - The IDT that the interrupt vector will be loaded in.
// Vector  - The interrupt number that the handler will be assigned to.
static void KepLoadInterruptVector(IDT* Idt, int Vector, KepInterruptVector Handler)
{
	IDTEntry* Entry = &Idt->Entries[Vector];
	memset(Entry, 0, sizeof * Entry);
	
	uintptr_t HandlerAddr = (uintptr_t) Handler;
	
	Entry->OffsetLow  = HandlerAddr & 0xFFFF;
	Entry->OffsetHigh = HandlerAddr >> 16;
	
	// The code segment that the interrupt handler will run in.
	Entry->SegmentSel = SEG_RING_0_CODE;
	
	// Ist = 0 means the stack is not switched when the interrupt keeps the CPL the same.
	// The IST should only be used in case of a fatal exception, such as a double fault.
	Entry->IST = 0;
	
	// The ring that the interrupt can be called from. It's usually 0 (so you can't fake
	// an exception from user mode), however, it can be set to 3 for userspace system calls.
	// Usually we use `syscall` or `sysenter` for that, though.
	Entry->DPL = 0;
	
	// This is an interrupt gate.
	Entry->GateType = 0xE;
	
	// The entry is present.
	Entry->Present = true;
}

extern void KepDoubleFaultHandler();
extern void KepPageFaultHandler();
extern void KepDpcIpiHandler();
extern void KepClockIrqHandler();

static void KepSetupIdt(IDT* Idt)
{
	struct
	{
		uint16_t Length;
		uint64_t Pointer;
	}
	PACKED IdtDescriptor;
	
	IdtDescriptor.Length  = sizeof   * Idt;
	IdtDescriptor.Pointer = (uint64_t) Idt;
	
	// Load interrupt vectors.
	KepLoadInterruptVector(Idt, INTV_DBL_FAULT,  KepDoubleFaultHandler);
	KepLoadInterruptVector(Idt, INTV_PAGE_FAULT, KepPageFaultHandler);
	KepLoadInterruptVector(Idt, INTV_DPC_IPI,    KepDpcIpiHandler);
	KepLoadInterruptVector(Idt, INTV_APIC_TIMER, KepClockIrqHandler);
	
	ASM("lidt (%0)"::"r"(&IdtDescriptor));
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

static void KePlaceholderHandler(CPUState* State)
{
	LogMsg("Error, KePlaceholderHandler isn't actually supposed to run!");
}

void KeInitCPU()
{
	int ispPFN = MmAllocatePhysicalPage();
	int idtPFN = MmAllocatePhysicalPage();
	
	if (ispPFN == PFN_INVALID || idtPFN == PFN_INVALID)
	{
		// TODO: crash
		LogMsg("Error, can't initialize CPU %u, we don't have enough memory. Tried to create IDT & interrupt stack", KeGetCPU()->m_apicID);
		KeStopCurrentCPU();
	}
	
	KeArchData* Data = KeGetData();
	memset(&Data->Gdt, 0, sizeof Data->Gdt);
	
	IDT* idt = MmGetHHDMOffsetAddr(MmPFNToPhysPage(idtPFN));
	memset(idt, 0, sizeof * idt);
	
	void* intStack = MmGetHHDMOffsetAddr(MmPFNToPhysPage(ispPFN));
	Data->IntStack = intStack;
	memset(intStack, 0, 4096);
	
	KepSetupTss(&Data->Tss);
	KepSetupGdt(Data);
	KepSetupIdt(idt);
	Data->Idt = idt;
	
	// Load the default ISR places with a placeholder.
	for (int i = 0; i < INT_COUNT; i++)
		KeAssignISR(i, KePlaceholderHandler);
}
