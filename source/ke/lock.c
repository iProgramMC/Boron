// Boron64 - Spin locks
#include <ke.h>
#include <arch.h>

bool KeTryLock(SpinLock* pLock)
{
	return !AtTestAndSetMO(pLock->m_bLocked, ATOMIC_MEMORD_ACQUIRE);
}

void KeLock(SpinLock* pLock)
{
	while (true)
	{
		if (KeTryLock(pLock))
			return;
		
		while (AtLoadMO(pLock->m_bLocked, ATOMIC_MEMORD_ACQUIRE))
			KeInterruptHint();
	}
}

void KeUnlock(SpinLock* pLock)
{
	AtClearMO(pLock->m_bLocked, ATOMIC_MEMORD_RELEASE);
}
