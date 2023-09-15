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
void KeSetCurrentPageTable(uintptr_t pt);

// CPU initialization function
void KeInitCPU(); // initializes the current CPU

// ==== Interrupt priority level ====
void KeOnUpdateIPL(eIPL newIPL, eIPL oldIPL);

// Architecture specific data
KeArchData* KeGetData();

#endif//NS64_ARCH_H
