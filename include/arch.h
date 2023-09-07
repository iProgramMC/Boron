#ifndef NS64_ARCH_H
#define NS64_ARCH_H

#include <main.h>

// ==== Platform specific defintions ====
#ifdef TARGET_AMD64
#include <arch/amd64.h>
#endif
// ==== Forward declarations. Depending on the platform, we'll include platform specific definitions. ====
typedef struct CPUState CPUState; // List of registers.

// Functions that do different things based on architecture,
// but exist everywhere
void KeWaitForNextInterrupt(void);
void KeInvalidatePage(void* page);
void KeSetInterruptsEnabled(bool b);      // no tracking of previous state currently. This does the relevant instruction
void KeInterruptHint(void);
void KeSetCPUPointer(void* pGS);
void*KeGetCPUPointer(void);
uintptr_t KeGetCurrentPageTable(void);

// CPU initialization function
void KeInitCPU(); // initializes the current CPU

// ==== Interrupts ====
void KeOnUpdateIPL(eIPL newIPL, eIPL oldIPL);

// interrupt service routines
typedef void(*InterruptServiceRoutine)(CPUState* pState);
void KeAssignISR(int type, InterruptServiceRoutine routine); // type is an eInterruptType


// ==== Interrupt Type ====
// Note! These aren't interrupt _vectors_, they're interrupt types.
enum eInterruptType
{
	INT_UNKNOWN,
	INT_DOUBLE_FAULT,
	INT_PAGE_FAULT,
	INT_IPI,
	//....
	INT_COUNT
};

// Architecture specific data
KeArchData* KeGetData();

#endif//NS64_ARCH_H
