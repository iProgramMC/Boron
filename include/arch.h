#ifndef NS64_ARCH_H
#define NS64_ARCH_H

#include <main.h>

// ==== Platform specific defintions ====
#ifdef TARGET_AMD64
#include <arch/amd64.h>
#endif
// ==== Forward declarations. Depending on the platform, we'll include platform specific definitions. ====
typedef struct KREGISTERS_tag KREGISTERS, *PKREGISTERS; // List of registers.

// Functions that do different things based on architecture,
// but exist everywhere
void KeWaitForNextInterrupt(void);
void KeInvalidatePage(void* page);
void KeSpinningHint(void);
void KeSetCPUPointer(void* pGS);
void*KeGetCPUPointer(void);
uintptr_t KeGetCurrentPageTable(void);
void KeSetCurrentPageTable(uintptr_t pt);
bool KeSetInterruptsEnabled(bool b); // returns old state

// CPU initialization function
void KeInitCPU(); // initializes the current CPU

// ==== Interrupt priority level ====
void KeOnUpdateIPL(KIPL newIPL, KIPL oldIPL);

// Architecture specific data
KARCH_DATA* KeGetData();

// ==== Execution Control ====
NO_RETURN void KeJumpContext(PKREGISTERS Registers);

#endif//NS64_ARCH_H
