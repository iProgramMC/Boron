// Boron64 - Exception handling
#ifndef NS64_EXCEPT_H
#define NS64_EXCEPT_H

#include <main.h>

void KeOnUnknownInterrupt(PKREGISTERS);
void KeOnDoubleFault(PKREGISTERS);
void KeOnProtectionFault(PKREGISTERS);
void KeOnPageFault(PKREGISTERS);

#endif//NS64_EXCEPT_H
