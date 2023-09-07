// Boron - Hardware abstraction for x86_64.
// In the future this module is planned to be linked separately to the kernel.
#ifndef NS64_HAL_H
#define NS64_HAL_H

#include <main.h>

// ==== Terminal ====
void HalTerminalInit(void);
void HalDebugTerminalInit(void);
void HalPrintString(const char* str);
void HalPrintStringDebug(const char* str);

#endif//NS64_HAL_H
