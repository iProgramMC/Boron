#include <ke.h>

int KeWaitForMultipleObjects(
	int Count,
	PKDISPATCH_HEADER Objects[],
	int WaitType,
	bool Alertable,
	PKWAIT_BLOCK WaitBlockArray)
{
	PKTHREAD Thread = KeGetCurrentThread();
	
	int Maximum = MAXIMUM_WAIT_BLOCKS;
	
	if (!WaitBlockArray)
	{
		WaitBlockArray = Thread->WaitBlocks;
		Maximum = THREAD_WAIT_BLOCKS;
	}
	
	if (Count > Maximum)
		KeCrash("KeWaitForMultipleObjects: Object count %d is bigger than the maximum wait blocks of %d", Count, Maximum); 
	
	// Raise the IPL so that we do not get interrupted while modifying the thread's wait parameters
	KIPL Ipl = KeRaiseIPL(IPL_DPC);
	
	Thread->WaitBlockArray = WaitBlockArray;
	Thread->WaitType = WaitType;
	Thread->WaitCount = Count;
	Thread->WaitIsAlertable = Alertable;
	Thread->WaitStatus = STATUS_WAITING;
	
	for (int i = 0; i < Count; i++)
	{
		PKWAIT_BLOCK WaitBlock = &WaitBlockArray[i];
		
		WaitBlock->Thread = Thread;
		WaitBlock->Object = Objects[i];
		
		// The thread's wait block should be part of the object's waiting object linked list.
		InsertTailList(&WaitBlock->Object->WaitBlockList, &WaitBlock->Entry);
	}
	
	Thread->Status = KTHREAD_STATUS_WAITING;
	
	KeLowerIPL(Ipl);
	
	KeYieldCurrentThread();
	
	return Thread->WaitStatus;
}

int KeWaitForSingleObject(PKDISPATCH_HEADER Object, bool Alertable)
{
	return KeWaitForMultipleObjects(1, &Object, WAIT_TYPE_ANY, Alertable, NULL);
}

void KeSatisfyWaitBlock(PKWAIT_BLOCK WaitBlock)
{
	PKTHREAD Thread = WaitBlock->Thread;
	int Index = WaitBlock - Thread->WaitBlockArray;
	
	bool WakeUp = false;
	
	if (Thread->WaitType == WAIT_TYPE_ANY)
	{
		WakeUp = true;
		Thread->WaitStatus = STATUS_RANGE_WAIT + Index;
	}
	else if (Thread->WaitType == WAIT_TYPE_ALL)
	{
		// Optimistically assume we already can wake up the thread
		WakeUp = true;
		
		// Check if all other objects are signaled as well
		for (int i = 0; i < Thread->WaitCount; i++)
		{
			PKWAIT_BLOCK WaitBlock = &Thread->WaitBlockArray[i];
			if (!WaitBlock->Object->Signaled)
			{
				WakeUp = false;
				break;
			}
		}
	}
	
	if (WakeUp)
		KeWakeUpThread(Thread);
}

void KeSignalObject(PKDISPATCH_HEADER Object)
{
	Object->Signaled = true;
	
	PLIST_ENTRY Entry = Object->WaitBlockList.Flink;
	
	while (Entry != &Object->WaitBlockList)
	{
		PKWAIT_BLOCK WaitBlock = CONTAINING_RECORD(Entry, KWAIT_BLOCK, Entry);
		
		KeSatisfyWaitBlock(WaitBlock);
		
		Entry = Entry->Flink;
	}
}
