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

// ==== CPU intrinsics ====
void HalWaitForNextInterrupt(void);
void HalInvalidatePage(void* page);
void HalSetInterruptsEnabled(bool b);      // no tracking of previous state currently. This does the relevant instruction
void HalSetCurrentPageTable(uintptr_t pa); // set the value of the CR3 register
uintptr_t HalGetCurrentPageTable(void);    // get the value of the CR3 register
void HalInterruptHint(void);
void HalSetCPUPointer(void* pGS);          // sets the kernel's GS base
void*HalGetCPUPointer(void);               // gts the kernel's GS base

// ==== Interrupts ====
#include <ke/ipl.h>
void HalOnUpdateIPL(eIPL newIPL, eIPL oldIPL);

// interrupt service routines
typedef void(*InterruptServiceRoutine)(CPUState* pState);
void HalAssignISR(int vector, InterruptServiceRoutine routine);

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

#endif//NS64_HAL_H
