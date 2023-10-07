// Boron64 - Interrupt management
#include <arch.h>
#include <string.h>

// The trap gate isn't likely to be used as it doesn't turn off
// interrupts when entering the interrupt handler.
enum KGATE_TYPE
{
	GATE_INT  = 0xE,
	GATE_TRAP = 0xF
};

// The IDT itself.
KIDT KiIdt;

struct KIDT_DESCRIPTOR_tag
{
	uint16_t Limit;
	KIDT*    Base;
}
PACKED
KiIdtDescriptor;

// The interrupt vector function type.  It's not a function you can actually call from C.
typedef void(*KiInterruptVector)();

// Probably not going to be used because SYSCALL and SYSENTER both bypass the interrupt system.
// These are going to be optimized out, and are also niceties when needed, so I will keep them.
static UNUSED void KiSetInterruptDPL(KIDT* Idt, int Vector, int Ring)
{
	Idt->Entries[Vector].DPL = Ring;
}

// This is also not likely to be used. We need interrupts to be disabled automatically
// before we raise the IPL to the interrupt's level, but trap gates will not disable interrupts.
// This is bad, since a keyboard interrupt could manage to sneak past an important clock interrupt
// before we manage to raise the IPL in the clock interrupt.
// Not to mention the performance gains would be minimal if at all existent.
static UNUSED void KiSetInterruptGateType(KIDT* Idt, int Vector, int GateType)
{
	Idt->Entries[Vector].GateType = GateType;
}

// Set the IST of an interrupt vector. This is probably useful for the double fault handler,
// as it is triggered only when another interrupt failed to be called, which is useful in cases
// where the kernel stack went missing and bad (Which I hope there aren't any!)
static UNUSED void KiSetInterruptStackIndex(KIDT* Idt, int Vector, int Ist)
{
	Idt->Entries[Vector].IST = Ist;
}

// This loads the interrupt vector handler into the IDT.
// Parameters:
// Idt     - The IDT that the interrupt vector will be loaded in.
// Vector  - The interrupt number that the handler will be assigned to.
static void KiLoadInterruptVector(KIDT* Idt, int Vector, KiInterruptVector Handler)
{
	KIDT_ENTRY* Entry = &Idt->Entries[Vector];
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

extern void KiTrapUnknown();
extern void KiTrap08();
extern void KiTrap0D();
extern void KiTrap0E();
extern void KiTrap40();
extern void KiTrapF0();
extern void KiTrapFD();
extern void KiTrapFE();
extern void KiTrapFF();

// Run on the BSP only.
void KiSetupIdt()
{
	KiIdtDescriptor.Base  = &KiIdt;
	KiIdtDescriptor.Limit = sizeof(KiIdt) - 1;
	
	for (int i = 0; i < C_IDT_MAX_ENTRIES; i++)
		KiLoadInterruptVector(&KiIdt, i, KiTrapUnknown);
	
	KiLoadInterruptVector(&KiIdt, 0x08, KiTrap08); // double faults
	KiLoadInterruptVector(&KiIdt, 0x0D, KiTrap0D); // general protection faults
	KiLoadInterruptVector(&KiIdt, 0x0E, KiTrap0E); // page faults
	KiLoadInterruptVector(&KiIdt, 0x40, KiTrap40); // DPC IPI 
	KiLoadInterruptVector(&KiIdt, 0xF0, KiTrapF0); // APIC timer interrupt
	KiLoadInterruptVector(&KiIdt, 0xFD, KiTrapFD); // crash IPI
	KiLoadInterruptVector(&KiIdt, 0xFE, KiTrapFE); // crash IPI
	KiLoadInterruptVector(&KiIdt, 0xFF, KiTrapFF); // spurious interrupts
}
