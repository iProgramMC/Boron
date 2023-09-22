// Boron - Kernel
#ifndef NS64_KE_H
#define NS64_KE_H

#include <main.h>
#include <_limine.h>
#include <arch.h>

// === Interrupt priority levels ===

// Raises or lowers the IPL of the current CPU.
// Returns the previous IPL.

// The reason I didn't opt for a KeIPLSet is because it
// could help create dangerous programming patterns.

// To raise the IPL temporarily, opt for the following structure;
//     eIPL oldIPL = KeRaiseIPL(IPL_WHATEVER);
//     .... your code here
//     KeLowerIPL(oldIPL);
// .. and the reverse for lowering the IPL (but you really should not)
eIPL KeRaiseIPL(eIPL newIPL);
eIPL KeLowerIPL(eIPL newIPL);
eIPL KeGetIPL();

// === Locking ===

// simple spin locks, for when contention is rare
typedef struct
{
	bool Locked;
}
SpinLock;

void KeLock(SpinLock* pLock);
void KeUnlock(SpinLock* pLock);
bool KeTryLock(SpinLock* pLock);

// ticket locks, for when contention is definitely possible
typedef struct
{
	int NowServing;
	int NextNumber;
}
TicketLock;

void KeInitTicketLock(TicketLock* pLock);
void KeLockTicket(TicketLock* pLock);
void KeUnlockTicket(TicketLock* pLock);

// === CPU ===
#include <ke/prcb.h>

// === Atomics ===

// If our compiler claims we're hosted, we're really not..
#if __STDC_HOSTED__ || defined(__STDC_NO_ATOMICS__)
#error "Hey"
#endif

#define ATOMIC_MEMORD_SEQ_CST __ATOMIC_SEQ_CST
#define ATOMIC_MEMORD_ACQ_REL __ATOMIC_ACQ_REL
#define ATOMIC_MEMORD_ACQUIRE __ATOMIC_ACQUIRE
#define ATOMIC_MEMORD_RELEASE __ATOMIC_RELEASE
#define ATOMIC_MEMORD_CONSUME __ATOMIC_CONSUME
#define ATOMIC_MEMORD_RELAXED __ATOMIC_RELAXED

// TODO: Allow default atomic memory order to be changed?
#define ATOMIC_DEFAULT_MEMORDER ATOMIC_MEMORD_SEQ_CST

// Use this if you want to use atomic intrinsics to access a variable.
#define ATOMIC _Atomic

// this is kind of ugly, but it'll work
#define AtLoad(var)                  __atomic_load_n (&(var), ATOMIC_DEFAULT_MEMORDER)
#define AtLoadMO(var, mo)            __atomic_load_n (&(var), mo)
#define AtStore(var, val)            __atomic_store_n(&(var), (val), ATOMIC_DEFAULT_MEMORDER)
#define AtStoreMO(var, val, mo)      __atomic_store_n(&(var), (val), mo)
#define AtTestAndSet(var)            __atomic_test_and_set(&(var), ATOMIC_DEFAULT_MEMORDER)             // test and set
#define AtTestAndSetMO(var,mo)       __atomic_test_and_set(&(var), mo)
#define AtClear(var)                 __atomic_clear(&(var), ATOMIC_DEFAULT_MEMORDER)
#define AtClearMO(var, mo)           __atomic_clear(&(var), mo)
#define AtFetchAdd(var, val)         __atomic_fetch_add(&(var), (val), ATOMIC_DEFAULT_MEMORDER)
#define AtFetchAddMO(var,val,mo)     __atomic_fetch_add(&(var), (val), mo)
#define AtAddFetch(var, val)         __atomic_add_fetch(&(var), (val), ATOMIC_DEFAULT_MEMORDER)
#define AtAddFetchMO(var,val,mo)     __atomic_add_fetch(&(var), (val), mo)
#define AtExchange(var,val,ret)      __atomic_exchange(&(var), &(val), &(ret), ATOMIC_DEFAULT_MEMORDER)
#define AtExchangeMO(var,val,ret,mo) __atomic_exchange(&(var), &(val), &(ret), mo)

// === SMP ===
NO_RETURN void KeInitSMP(void);

struct KDPC_tag;

// === Deferred procedure calls ===
// TODO
typedef void(*KDeferredProcedure)(struct KDPC_tag*, void* pContext, void* pSysArg1, void* pSysArg2);

typedef struct KDPC_tag
{
	KDeferredProcedure m_deferredProcedure;
	void*              m_pDeferredContext;
	void*              m_pSystemArgument1;
	void*              m_pSystemArgument2;
}
KDeferredProcCall;

// === Crashing ===
NO_RETURN void KeCrash(const char* message, ...);
NO_RETURN void KeCrashBeforeSMPInit(const char* message, ...);

#endif//NS64_KE_H