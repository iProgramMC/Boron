// Boron - Hardware abstraction for x86_64.
// In the future this module is planned to be linked separately to the kernel.
#ifndef NS64_HAL_H
#define NS64_HAL_H

#include <main.h>

// ==== Forward declarations. Depending on the platform, we'll include platform specific definitions. ====
typedef struct CPUState CPUState; // List of registers.

// ==== Terminal ====
void HalTerminalInit(void);
void HalDebugTerminalInit(void);
void HalPrintString(const char* str);
void HalPrintStringDebug(const char* str);

// ==== Initialization ====
void KeInitCPU(); // initializes the current CPU

// ==== CPU intrinsics ====
void KeWaitForNextInterrupt(void);
void KeInvalidatePage(void* page);
void KeSetInterruptsEnabled(bool b);      // no tracking of previous state currently. This does the relevant instruction
void KeSetCurrentPageTable(uintptr_t pa); // set the value of the CR3 register
uintptr_t KeGetCurrentPageTable(void);    // get the value of the CR3 register
void KeInterruptHint(void);
void KeSetCPUPointer(void* pGS);          // sets the kernel's GS base
void*KeGetCPUPointer(void);               // gts the kernel's GS base

// ==== Interrupts ====
#include <ke/ipl.h>
void KeOnUpdateIPL(eIPL newIPL, eIPL oldIPL);

// interrupt service routines
typedef void(*InterruptServiceRoutine)(CPUState* pState);
void KeAssignISR(int type, InterruptServiceRoutine routine); // type is an eInterruptType

// ==== Platform specific defintions ====
#ifdef TARGET_AMD64
#include <hal/amd64/pd.h>
#endif

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

KeArchData* KeGetData();

#endif//NS64_HAL_H
