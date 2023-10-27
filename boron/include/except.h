// Boron64 - Exception handling
#ifndef NS64_EXCEPT_H
#define NS64_EXCEPT_H

void KeOnUnknownInterrupt(uintptr_t FaultPC, uintptr_t Vector);
void KeOnDoubleFault(uintptr_t FaultPC);
void KeOnProtectionFault(uintptr_t FaultPC);
void KeOnPageFault(uintptr_t FaultPC, uintptr_t FaultAddress, uintptr_t FaultMode);

#endif//NS64_EXCEPT_H
