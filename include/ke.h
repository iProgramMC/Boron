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

// === SMP ===
NO_RETURN void KeInitSMP(void);

// === Crashing ===
NO_RETURN void KeCrash(const char* message, ...);
NO_RETURN void KeCrashBeforeSMPInit(const char* message, ...);

#endif//NS64_KE_H