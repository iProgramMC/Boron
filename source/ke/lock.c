// Boron64 - Spin locks
#include <ke.h>
#include <arch.h>

bool KeTryLock(SpinLock* pLock)
{
	return !AtTestAndSetMO(pLock->Locked, ATOMIC_MEMORD_ACQUIRE);
}

void KeLock(SpinLock* pLock)
{
	while (true)
	{
		if (KeTryLock(pLock))
			return;
		
		while (AtLoadMO(pLock->Locked, ATOMIC_MEMORD_ACQUIRE))
			KeSpinningHint();
	}
}

void KeUnlock(SpinLock* pLock)
{
	AtClearMO(pLock->Locked, ATOMIC_MEMORD_RELEASE);
}

void KeInitTicketLock(TicketLock* pLock)
{
	AtClear(pLock->NowServing);
	AtClear(pLock->NextNumber);
}

void KeLockTicket(TicketLock* pLock)
{
	int MyTicket = AtFetchAddMO(pLock->NextNumber, 1, ATOMIC_MEMORD_ACQUIRE);
	
	while (AtLoadMO(pLock->NowServing, ATOMIC_MEMORD_ACQUIRE) != MyTicket)
		KeSpinningHint();
}

void KeUnlockTicket(TicketLock* pLock)
{
	AtAddFetchMO(pLock->NowServing, 1, ATOMIC_MEMORD_RELEASE);
}
