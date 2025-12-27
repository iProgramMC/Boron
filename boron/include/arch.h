#ifndef NS64_ARCH_H
#define NS64_ARCH_H

#include <main.h>

// ==== Platform specific defintions ====
#if   defined TARGET_AMD64
#include <arch/amd64.h>
#elif defined TARGET_I386
#include <arch/i386.h>
#elif defined TARGET_ARM
#include <arch/arm.h>
#else
#error Define your architecture here!
#endif

// ==== Forward declarations. Depending on the platform, we'll include platform specific definitions. ====
typedef struct KREGISTERS_tag KREGISTERS, *PKREGISTERS; // List of registers.

// Functions that do different things based on architecture,
// but exist everywhere
void KeWaitForNextInterrupt(void);
void KeSpinningHint(void);
void KeInvalidatePage(void* Page);
void KeSetCPUPointer(void* CpuPointer);
void* KeGetCPUPointer(void);
uintptr_t KeGetCurrentPageTable(void);
void KeFlushTLB(void);
void KeSetCurrentPageTable(uintptr_t PageTable);
bool KeDisableInterrupts(); // returns old state
void KeRestoreInterrupts(bool OldState);

// CPU initialization function
void KeInitCPU(); // initializes the current CPU

// ==== Interrupt priority level ====
void KeOnUpdateIPL(KIPL newIPL, KIPL oldIPL);

void KeIssueTLBShootDown(uintptr_t Address, size_t Length);

// Architecture specific data
KARCH_DATA* KeGetData();

// ==== Execution Control ====
//Don't use - reimplement if you need to: NO_RETURN void KeJumpContext(PKREGISTERS Registers);

#endif//NS64_ARCH_H
