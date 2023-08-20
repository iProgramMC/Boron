// Boron - Hardware abstraction for x86_64.
// In the future this module is planned to be linked separately to the kernel.
#ifndef NS64_HAL_H
#define NS64_HAL_H

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

#endif//NS64_HAL_H
