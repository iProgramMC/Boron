/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	rtl/armv5sup.c
	
Abstract:
	This module implements several atomic methods for armv5.
	
Author:
	iProgramInCpp - 31 January 2026
***/
#include <main.h>

#ifdef TARGET_ARMV5

#ifdef KERNEL

#include <ke.h>

void __sync_synchronize()
{
	unsigned int zero = 0;
	// Data Memory Barrier
	ASM("mcr p15, 0, %0, c7, c10, 5" : : "r" (zero) : "memory");
}

#define LockAtomics() KeDisableInterrupts()
#define UnlockAtomics(x) KeRestoreInterrupts(x)

#else

// TODO: implement user-mode atomics.  Multithreaded user mode applications
// will likely be unstable until then.
void __sync_synchronize()
{
}

#define LockAtomics() true
#define UnlockAtomics(x) ((void)x)

#endif

unsigned long long __atomic_load_8(const volatile void* PointerV, UNUSED int MemoryOrder)
{
	bool Restore = LockAtomics();
	const volatile unsigned long long* Pointer = PointerV;
	unsigned long long Result = *Pointer;
	UnlockAtomics(Restore);
	return Result;
}

unsigned long long __atomic_fetch_add_8(volatile void* PointerV, unsigned long long Value, UNUSED int MemoryOrder)
{
	bool Restore = LockAtomics();
	volatile unsigned long long* Pointer = PointerV;
	unsigned long long Result = *Pointer;
	*Pointer += Value;
	UnlockAtomics(Restore);
	return Result;
}

unsigned int __atomic_fetch_add_4(volatile void* PointerV, unsigned int Value, UNUSED int MemoryOrder)
{
	bool Restore = LockAtomics();
	volatile unsigned int* Pointer = PointerV;
	unsigned int Result = *Pointer;
	*Pointer += Value;
	UnlockAtomics(Restore);
	return Result;
}

unsigned short __atomic_fetch_add_2(volatile void* PointerV, unsigned short Value, UNUSED int MemoryOrder)
{
	bool Restore = LockAtomics();
	volatile unsigned short* Pointer = PointerV;
	unsigned short Result = *Pointer;
	*Pointer += Value;
	UnlockAtomics(Restore);
	return Result;
}

unsigned int __atomic_fetch_or_4(volatile void* PointerV, unsigned int Value, UNUSED int MemoryOrder)
{
	bool Restore = LockAtomics();
	volatile unsigned int* Pointer = PointerV;
	unsigned int Result = *Pointer;
	*Pointer |= Value;
	UnlockAtomics(Restore);
	return Result;
}

unsigned int __atomic_fetch_and_4(volatile void* PointerV, unsigned int Value, UNUSED int MemoryOrder)
{
	bool Restore = LockAtomics();
	volatile unsigned int* Pointer = PointerV;
	unsigned int Result = *Pointer;
	*Pointer &= Value;
	UnlockAtomics(Restore);
	return Result;
}

bool __atomic_compare_exchange_4(
	volatile void* DestV,
	void* ExpectedV,
	unsigned int Desired,
	UNUSED bool Weak,
	UNUSED int SuccessMemoryOrder,
	UNUSED int FailureMemoryOrder
)
{
	bool Restore = LockAtomics();
	bool Result = false;
	volatile unsigned int* Dest = DestV;
	volatile unsigned int* Expected = ExpectedV;
	
	if (*Dest == *Expected)
	{
		*Dest = Desired;
		Result = true;
	}
	else
	{
		*Expected = *Dest;
		Result = false;
	}
	
	UnlockAtomics(Restore);
	return Result;
}

#endif
