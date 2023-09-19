// Boron - Hardware abstraction for x86_64.
// In the future this module is planned to be linked separately to the kernel.
#ifndef NS64_HAL_H
#define NS64_HAL_H

#include <main.h>
#include <arch.h>

// ==== Terminal ====
// Warning: Only run these on the BSP
void HalTerminalInit(void);
void HalDebugTerminalInit(void);

// Warning: You need to lock g_PrintLock to use this:
void HalPrintString(const char* str);

// Warning: You need to lock g_DebugPrintLock to use this:
void HalPrintStringDebug(const char* str);

// ==== TLB Shootdown ====
void HalIssueTLBShootDown(uintptr_t Address, size_t Length);

// ==== Crashing ====
// don't use. Use KeCrash instead
NO_RETURN void HalCrashSystem(const char* message);

// ==== AMD64 specific features ====
#ifdef TARGET_AMD64
void HalEnableApic();
#endif

#endif//NS64_HAL_H
