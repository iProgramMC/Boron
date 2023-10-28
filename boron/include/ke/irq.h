// Boron - interrupts
#ifndef NS64_KE_IRQ_H
#define NS64_KE_IRQ_H

#include <ke/ipl.h>
#include <arch.h>

// Note - This API is to be used during single processor initialization (UP-Init) only.
// Behavior is not defined during MP-Init initialization.

typedef PKREGISTERS(*PKINTERRUPT_HANDLER)(PKREGISTERS);

int KeAllocateInterruptVector(KIPL Ipl);

void KeRegisterInterrupt(int Vector, PKINTERRUPT_HANDLER Handler);

void KeSetInterruptIPL(int Vector, KIPL Ipl);

#endif//NS64_KE_IRQ_H
