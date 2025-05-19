// Boron - Hardware abstraction for x86_64.
// In the future this module is planned to be linked separately to the kernel.
#ifndef NS64_HAL_H
#define NS64_HAL_H

#include <main.h>
#include <arch.h>

#include <hal/timer.h>
#include <hal/data.h>
#include <hal/init.h>

#define HAL_IPI_BROADCAST (1 << 0)
#define HAL_IPI_SELF      (1 << 1)

bool HalWasInitted();

// HAL API. See hal/init.h
void HalEndOfInterrupt();
void HalRequestIpi(uint32_t LapicId, uint32_t Flags, int Vector);
void HalInitSystemUP();
void HalInitSystemMP();
void HalDisplayString(const char* Message);
void HalCrashSystem(const char* Message);
bool HalUseOneShotIntTimer();
void HalProcessorCrashed() NO_RETURN;
uint64_t HalGetIntTimerFrequency();
uint64_t HalGetTickCount();
uint64_t HalGetTickFrequency();
uint64_t HalGetIntTimerDeltaTicks();
void HalIoApicSetIrqRedirect(uint8_t Vector, uint8_t Irq, uint32_t LapicId, bool Status);

#ifdef IS_HAL
void HalSetVftable(const HAL_VFTABLE* Table);
#endif


// Outdated and will be removed:


// ==== Terminal ====
// Warning: Only run these on the BSP
void HalInitTerminal(void);
bool HalIsTerminalInitted();

// Warning: You need to lock KiPrintLock to use this:
void HalPrintString(const char* str);

// Warning: You need to lock KiDebugPrintLock to use this:
void HalPrintStringDebug(const char* str);

// ==== Crashing ====
// don't use. Use KeCrash instead
NO_RETURN void HalCrashSystem(const char* message);

// ==== Initialization ====
void HalUPInit();
void HalMPInit();

// ==== AMD64 specific features ====
#ifdef TARGET_AMD64
void HalEnableApic();
#endif

#endif//NS64_HAL_H
