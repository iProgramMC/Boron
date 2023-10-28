// Boron - Kernel
#ifndef NS64_KE_H
#define NS64_KE_H

#include <main.h>
#include <_limine.h>
#include <arch.h>

// === Interrupt priority levels ===
#include <ke/ipl.h>

// === Locking ===
#include <ke/locks.h>

// === CPU ===
#include <ke/prcb.h>

// === Atomics ===
#include <ke/atomics.h>

// === Statistics ===
#include <ke/stats.h>

// === Interrupts ===
#include <ke/irq.h>

// === Debugging ===
void DbgPrintString(const char* str);
uintptr_t DbgLookUpAddress(const char* Name);
const char* DbgLookUpRoutineByAddress(uintptr_t Address);

// === SMP ===
NO_RETURN void KeInitSMP(void);
void KeInitArchUP();
void KeInitArchMP();

// === Crashing ===
NO_RETURN void KeCrash(const char* message, ...);
NO_RETURN void KeCrashBeforeSMPInit(const char* message, ...);


void KeIssueTLBShootDown(uintptr_t Address, size_t Length);


#endif//NS64_KE_H