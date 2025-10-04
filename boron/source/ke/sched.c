/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/sched.c
	
Abstract:
	This module contains the implementation of the scheduler.
	
Author:
	iProgramInCpp - 3 October 2023
***/
#include "ki.h"

//#define SCHED_DEBUG
#ifdef SCHED_DEBUG
#define SchedDebug(...) DbgPrint(__VA_ARGS__)
#define SchedDebug2(...) DbgPrint(__VA_ARGS__)
#else
#define SchedDebug(...)
#define SchedDebug2(...) DbgPrint(__VA_ARGS__)
#endif
 
#define MAX_THREADS_ON_QUEUE_OVERLOADED 100

//#define SCHED_DISABLE_WORKSTEALING

static int KepNextProcessorToStealWorkFrom;
extern PKPRCB* KeProcessorList;
extern int     KeProcessorCount;

LIST_ENTRY KiGlobalThreadList;
KSPIN_LOCK KiGlobalThreadListLock;

PKSCHEDULER KiGetCurrentScheduler()
{
#ifdef DEBUG
	if (KeGetIPL() < IPL_DPC)
	{
		bool Restore = KeDisableInterrupts();
		KeRestoreInterrupts(Restore);
		if (Restore)
			ASSERT(!"Interrupts not disabled OR IPL < IPL_DPC in KiGetCurrentScheduler");
	}
#endif
	
	return &KeGetCurrentPRCB()->Scheduler;
}

#ifndef TARGET_AMD64 // On AMD64, this is optimized into an inline GS-relative access.

PKTHREAD KeGetCurrentThread()
{
	// Disable interrupts in order to prevent being scheduled out.
	// This can also be done by raising and lowering IPL, but this
	// is expensive relative to simply disabling interrupts.
	//
	// We might end up on a different processor, and that PRCB might
	// have already had its CurrentThread pointer overwritten to what
	// we don't expect.
	
	bool Restore = KeDisableInterrupts();
	PKTHREAD Thread = KiGetCurrentScheduler()->CurrentThread;
	KeRestoreInterrupts(Restore);
	
	return Thread;
}

#endif
 
static bool   KiMoreAggressiveWorkStealing = false;
static PKPRCB KiMostCrowdedProcessor = NULL;

void KiCheckOverloadedExecQueues()
{
	KiAssertOwnDispatcherLock();
	
	KiMoreAggressiveWorkStealing = false;
	KiMostCrowdedProcessor = NULL;
	
	PKPRCB* PrcbList = KeProcessorList;
	for (int i = 0; i < KeProcessorCount; i++)
	{
		if (PrcbList[i]->Scheduler.ThreadsOnQueueCount >= MAX_THREADS_ON_QUEUE_OVERLOADED)
		{
			KiMoreAggressiveWorkStealing = true;
			
			if (!KiMostCrowdedProcessor ||
			    KiMostCrowdedProcessor->Scheduler.ThreadsOnQueueCount < PrcbList[i]->Scheduler.ThreadsOnQueueCount)
				KiMostCrowdedProcessor = PrcbList[i];
		}
	}
}

void KiSetPriorityThread(PKTHREAD Thread, int Priority)
{
	if (!IsListEmpty(&Thread->EntryQueue))
	{
		PKSCHEDULER Scheduler = KiGetCurrentScheduler();
		
		// Remove from the old place in its queue, and assign it to the new place in the queue.
		RemoveEntryList(&Thread->EntryQueue);
		
		// TODO: Assert that the thread entry queue was actually part of that queue for debugging purposes
		if (IsListEmpty(&Scheduler->ExecQueue[Thread->Priority]))
			Scheduler->ExecQueueMask &= ~QUEUE_BIT(Thread->Priority);
		
		if (Thread->Priority > Priority)
			InsertHeadList(&Scheduler->ExecQueue[Priority], &Thread->EntryQueue);
		else
			InsertTailList(&Scheduler->ExecQueue[Priority], &Thread->EntryQueue);
		
		Scheduler->ExecQueueMask |= QUEUE_BIT(Priority);
	}
	
	Thread->Priority = Priority;
	Thread->BasePriority = Priority;
	Thread->PriorityBoost = 0;
}

static NO_RETURN void KiIdleThreadEntry(UNUSED void* Context)
{
	/*
	if (KeGetCurrentPRCB()->IsBootstrap)
	{
		// Wait approximately 2 seconds such that all remaining init jobs have completed.
		//
		// TODO ^^ This is an arbitrary quantity.  Also it's possible that the init jobs
		// haven't actually completed by 2 seconds later!
		
		uint64_t TimeIn2Seconds = HalGetTickCount() + HalGetTickFrequency() * 2;
		while (HalGetTickCount() < TimeIn2Seconds)
			KeWaitForNextInterrupt();
		
		MiReclaimInitText();
	}
	*/
	while (true)
		KeWaitForNextInterrupt();
}

// Same as KeReadyThread, but the dispatcher is already locked
void KiReadyThread(PKTHREAD Thread)
{
	KiAssertOwnDispatcherLock();
	
	Thread->Status = KTHREAD_STATUS_READY;
	Thread->Suspended = false;
	
	PKSCHEDULER Scheduler = KiGetCurrentScheduler();
	
	KIPL Ipl;
	KeAcquireSpinLock(&KiGlobalThreadListLock, &Ipl);
	InsertTailList(&KiGlobalThreadList, &Thread->EntryGlobal);
	KeReleaseSpinLock(&KiGlobalThreadListLock, Ipl);
	
	InsertTailList(&Scheduler->ExecQueue[Thread->Priority], &Thread->EntryQueue);
	
	Thread->EnqueuedTime = HalGetTickCount();
	
	Scheduler->ThreadsOnQueueCount++;
	Scheduler->ExecQueueMask |= QUEUE_BIT(Thread->Priority);
	
	Thread->LastProcessor = KeGetCurrentPRCB()->Id;
	
	KiCheckOverloadedExecQueues();
}

KPRIORITY KiAdjustPriorityBoost(KPRIORITY BasePriority, KPRIORITY PriorityBoost)
{
	KPRIORITY NewPriority = BasePriority + PriorityBoost;
	
	// XXX I know this sucks... but yeah.
	if (BasePriority == PRIORITY_IDLE) NewPriority = PRIORITY_IDLE;
	else if (NewPriority > PRIORITY_REALTIME_MAX && PRIORITY_REALTIME_MAX >= BasePriority && BasePriority >= PRIORITY_REALTIME) NewPriority = PRIORITY_REALTIME_MAX;
	else if (NewPriority > PRIORITY_HIGH_MAX     && PRIORITY_HIGH_MAX     >= BasePriority && BasePriority >= PRIORITY_HIGH)     NewPriority = PRIORITY_HIGH_MAX;
	else if (NewPriority > PRIORITY_NORMAL_MAX   && PRIORITY_NORMAL_MAX   >= BasePriority && BasePriority >= PRIORITY_NORMAL)   NewPriority = PRIORITY_NORMAL_MAX;
	else if (NewPriority > PRIORITY_LOW_MAX      && PRIORITY_LOW_MAX      >= BasePriority && BasePriority >= PRIORITY_LOW)      NewPriority = PRIORITY_LOW_MAX;
	
	return NewPriority - BasePriority;
}

void KiUnwaitThread(PKTHREAD Thread, int Status, KPRIORITY Increment)
{
	KiAssertOwnDispatcherLock();
	ASSERT(Thread->Status != KTHREAD_STATUS_READY);
	
	PKSCHEDULER Scheduler = &KeProcessorList[Thread->LastProcessor]->Scheduler;
	
	Thread->Status = KTHREAD_STATUS_READY;
	
	// Adjust the priority boost so that it cannot escape its priority class.
	Increment = KiAdjustPriorityBoost(Thread->Priority, Increment);
	
	Thread->PriorityBoost = Increment;
	Thread->Priority = Thread->BasePriority + Increment;
	
	// Remove ourselves from all the objects' wait block lists
	for (int i = 0; i < Thread->WaitCount; i++)
	{
		PKWAIT_BLOCK WaitBlock = &Thread->WaitBlockArray[i];
		RemoveEntryList(&WaitBlock->Entry);
	}
	
	Thread->WaitCount = 0;
	Thread->WaitBlockArray = NULL;
	
	Thread->WaitStatus = Status;
	Thread->DontSteal = true;
	
	// Emplace ourselves on the execution queue.
	InsertTailList(&Scheduler->ExecQueue[Thread->Priority], &Thread->EntryQueue);
	Scheduler->ExecQueueMask |= QUEUE_BIT(Thread->Priority);
	
	Thread->EnqueuedTime = HalGetTickCount();
	
	KiCheckOverloadedExecQueues();
	
	KiCancelTimer(&Thread->WaitTimer);
	KiCancelTimer(&Thread->SleepTimer);
	
	// If the current thread is running at a lower priority than this one, yield
	// as soon as the dispatcher lock is unlocked.
	if (KeGetCurrentThread()->Priority < Thread->Priority)
	{
		KiSetPendingQuantumEnd();
	}
}

INIT
void KeSchedulerInitUP()
{
	InitializeListHead(&KiGlobalThreadList);
}

INIT
void KeSchedulerInit(void* IdleThreadStack)
{
	size_t IdleThreadStackSize = MmGetSizeFromPoolAddress(IdleThreadStack) * PAGE_SIZE;
	
	ASSERT(IdleThreadStack);
	
	KIPL Ipl = KiLockDispatcher();
	PKSCHEDULER Scheduler = KiGetCurrentScheduler();
	
	InitializeRbTree(&Scheduler->TimerTree);
	
	for (int i = 0; i < PRIORITY_COUNT; i++)
		InitializeListHead(&Scheduler->ExecQueue[i]);
	
	Scheduler->CurrentThread = NULL;
	
	Scheduler->ExecQueueMask = 0;
	Scheduler->ThreadsOnQueueCount = 0;
	
	// Create an idle thread.
	PKTHREAD Thread = KeAllocateThread();
	if (!Thread)
		KeCrash("cannot allocate idle thread");
	
	KiInitializeThread(Thread, IdleThreadStack, IdleThreadStackSize, KiIdleThreadEntry, NULL, KeGetSystemProcess());
	
	KiSetPriorityThread(Thread, PRIORITY_IDLE);
	KiReadyThread(Thread);
	
	KiUnlockDispatcher(Ipl);
}

// Commit to running threads managed by the scheduler.
NO_RETURN void KeSchedulerCommit()
{
	// NOTE: The raise of the IPL to IPL_DPC is not really because of anything
	// KiSetPendingQuantumEnd does being unsafe, it's to actually get it to issue
	// the quantum-end interrupt.
	KIPL Ipl = KeRaiseIPL(IPL_DPC);
	KiSetPendingQuantumEnd();
	KeLowerIPL(Ipl);
	
	// Wait for the waves to pick us up...
	while (true)
		KeWaitForNextInterrupt();
}

static PKTHREAD KepPopNextThreadIfNeeded(PKSCHEDULER Sched, int MinPriority, bool OtherProcessor)
{
	// If this is another processor, perform the ExecQueueMask optimization.
#ifndef SCHED_DISABLE_WORKSTEALING
	if (OtherProcessor)
	{
		// If the mask doesn't have bits at positions MinPriority or higher,
		// that means that this processor only contains lower priority threads,
		// therefore we should try focusing on our own.
		if (Sched->ExecQueueMask < QUEUE_BIT(MinPriority))
			return NULL;
		
		// If we are trying to grab an idle thread from another processor, I'm sorry,
		// but I cannot let that happen.  Although affinity should help us.
		if (MinPriority == 0)
			return NULL;
	}
#else
	if (OtherProcessor)
		return NULL;
#endif
	
	for (int Priority = PRIORITY_COUNT - 1; Priority >= MinPriority; Priority--)
	{
		// Is the list empty?
		if (IsListEmpty(&Sched->ExecQueue[Priority]))
			continue;
		
		// Pop off the thread.
		PLIST_ENTRY ListEntry = Sched->ExecQueue[Priority].Flink;
		PKTHREAD Thread = CONTAINING_RECORD(ListEntry, KTHREAD, EntryQueue);
		
		// Check if the thread's affinity mask contains this processor's ID.
		if (~Thread->Affinity & (1ULL << KeGetCurrentPRCB()->Id))
		{
			// Nope, therefore this thread is a no-go.
			// TODO: Check other threads within the queue?
			return NULL;
		}
		
		// Check if the thread isn't ready to be stolen yet.
		if (Thread->DontSteal && OtherProcessor)
		{
			// Nope, therefore this thread is a no-go.
			return NULL;
		}
		
		// Remove this thread from the queue.
		RemoveEntryList(&Thread->EntryQueue);
		
		Sched->ThreadsOnQueueCount--;
		KiCheckOverloadedExecQueues();
		
		// If the list is empty, unset the relevant bit.
		if (IsListEmpty(&Sched->ExecQueue[Priority]))
			Sched->ExecQueueMask &= ~QUEUE_BIT(Priority);
		
		// Clear the priority boost so that it doesn't persist.
		Thread->Priority = Thread->BasePriority;
		Thread->PriorityBoost = 0;
		
		return Thread;
	}
	
	// No thread with at least MinPriority priority was picked.
	return NULL;
}

void KiAssignDefaultQuantum(PKTHREAD Thread)
{
	KiAssertOwnDispatcherLock();
	
	uint64_t QuantumTicks = HalGetTickFrequency() * MAX_QUANTUM_US / 1000000;
	uint64_t QuantumUntil = HalGetTickCount() + QuantumTicks;
	
	Thread->QuantumUntil = QuantumUntil;
	KiGetCurrentScheduler()->QuantumUntil = QuantumUntil;
	
	if (HalUseOneShotIntTimer())
	{
		uint64_t Tick = HalGetIntTimerFrequency() * MAX_QUANTUM_US / 1000000;
		uint64_t TimerTick = KiGetNextTimerExpiryItTick();
		
		if (Tick > TimerTick && TimerTick)
			Tick = TimerTick;
		
		HalRequestInterruptInTicks(Tick);
	}
	else
	{
		// It's just going to show up
	}
}

PKTHREAD KiGetNextThread(bool MayDowngrade);

void KiEndThreadQuantum()
{
	KiAssertOwnDispatcherLock();
	
	// NOTE: This function is called only if the thread's quantum expired.
	// Thus, there's always going to be something running at at least that thread's
	// priority, including the current thread itself.
	
	PKSCHEDULER Scheduler = KiGetCurrentScheduler();
	PKTHREAD CurrentThread = Scheduler->CurrentThread;
	
	PKTHREAD NextThread = KiGetNextThread(false);
	if (!NextThread)
	{
		// If we don't have any more threads that have at least the priority
		// of the current one, return immediately. We are not going to switch.
		KiAssignDefaultQuantum(CurrentThread);
		return;
	}
	
	Scheduler->NextThread = NextThread;
	
	// Since the current thread's quantum has expired, it's probably ready to run again!
	if (CurrentThread)
	{
		if (CurrentThread->Suspended)
		{
			// Suspended thread -- do not add to any queue.
			CurrentThread->Status = KTHREAD_STATUS_INITIALIZED;
		}
		else
		{
			CurrentThread->Status = KTHREAD_STATUS_READY;
			
			SchedDebug("Emplacing Thread %p In Queue", CurrentThread);
			
			// Emplace it back on the ready queue.
			InsertTailList(&Scheduler->ExecQueue[CurrentThread->Priority], &CurrentThread->EntryQueue);
			
			Scheduler->ThreadsOnQueueCount++;
			Scheduler->ExecQueueMask |= QUEUE_BIT(CurrentThread->Priority);
		}
		
		// Set the last processor ID of the thread.
		CurrentThread->LastProcessor = KeGetCurrentPRCB()->Id;
		CurrentThread->EnqueuedTime = HalGetTickCount();
		
		KiCheckOverloadedExecQueues();
	}
	
	Scheduler->QuantumUntil  = 0;
}

static void KepCleanUpThread(UNUSED PKDPC Dpc, void* ContextV, UNUSED void* SystemArgument1, UNUSED void* SystemArgument2)
{
	KIPL Ipl = KiLockDispatcher();
	
	PKTHREAD Thread = ContextV;
	
	// Remove the thread from the global list of threads.
	KIPL Ipl2;
	KeAcquireSpinLock(&KiGlobalThreadListLock, &Ipl2);
	RemoveEntryList(&Thread->EntryGlobal);
	KeReleaseSpinLock(&KiGlobalThreadListLock, Ipl2);
	
	// Remove the thread from the parent process' list of threads.
	RemoveEntryList(&Thread->EntryProc);
	
	// TODO: Remove every mutex that this thread owned before it died.
	
	// Finally, signal all threads that are waiting on this thread.
	// Signaling NOW and not in KeTerminateThread prevents a race condition where
	// code that looks as follows could free the thread while it is still in the
	// scheduler's system, which is nasty.
	//
	// KeWaitForSingleObject(MyThread, false, TIMEOUT_INFINITE);
	// KeDeallocateThread(MyThread);
	//
	// The race condition is that the DPC could show up later than when we run the
	// KeDeallocateThread function, which is actually several layers of nasty.
	//   1. An invalid DPC stays in the DPC queue.
	//   2. An invalid thread is being cleaned up.
	//
	// Therefore we DO NOT wait-test a thread UNTIL it is ready.
	Thread->Header.Signaled = true;
	KiWaitTest(&Thread->Header, Thread->IncrementTerminated);
	
	// TODO: If the main thread died, send a kernel APC to all other threads to also terminate?
	
	// This needs to be called with the dispatcher lock NOT held.
	PKTHREAD_TERMINATE_METHOD ThreadTerminate = Thread->TerminateMethod;
	
	// If the process' list of threads is empty, signal the process object.
	if (Thread->Process && IsListEmpty(&Thread->Process->ThreadList))
		KiOnKillProcess(Thread->Process);
	
	KiUnlockDispatcher(Ipl);
	
	// If the thread has a terminate method, call it.
	if (ThreadTerminate)
		ThreadTerminate(Thread);
}

// I dub this "work stealing", although I'm pretty sure I've heard this somewhere before.
// If a processor doesn't have any more threads at the current priority level or higher,
// it will try to pop threads off of another processor's queue.
static int KepGetNextProcessorToStealWorkFrom()
{
	KiAssertOwnDispatcherLock();
	
	if (KiMoreAggressiveWorkStealing)
		return KiMostCrowdedProcessor->Id;

	int Processor = KepNextProcessorToStealWorkFrom;
	
	if (++KepNextProcessorToStealWorkFrom == KeProcessorCount)
		KepNextProcessorToStealWorkFrom = 0;
	
	return Processor;
}

static PKTHREAD KepTryStealThread(int MinPriority)
{
	// Try "stealing" one of another processor's higher priority threads.
	//
	// N.B. Since we are currently using a big scheduler lock instead of many smaller locks,
	// there is essentially a guarantee that no one else will be messing with the execution
	// queue while we are.
	int ProcToSteal = KepGetNextProcessorToStealWorkFrom();
	PKSCHEDULER TheirScheduler = &KeProcessorList[ProcToSteal]->Scheduler;
	
	return KepPopNextThreadIfNeeded(TheirScheduler, MinPriority + 1, true);
}

static void KepStealManyThreads()
{
	KiAssertOwnDispatcherLock();
	
	PKSCHEDULER Scheduler = KiGetCurrentScheduler();
	
	int ProcToSteal = KepGetNextProcessorToStealWorkFrom();
	PKSCHEDULER TheirScheduler = &KeProcessorList[ProcToSteal]->Scheduler;
	
	int LeftToSteal = TheirScheduler->ThreadsOnQueueCount / 2;
	//DbgPrint("%d stealing from %d, %d", KeGetCurrentPRCB()->Id, ProcToSteal, LeftToSteal);
	
	for (int i = PRIORITY_COUNT - 1; i > 0 && LeftToSteal > 0; i--)
	{
		while (!IsListEmpty(&TheirScheduler->ExecQueue[i]) && LeftToSteal > 0)
		{
			PLIST_ENTRY Entry = RemoveHeadList(&TheirScheduler->ExecQueue[i]);
			InsertTailList(&Scheduler->ExecQueue[i], Entry);
			
			if (IsListEmpty(&TheirScheduler->ExecQueue[i]))
				TheirScheduler->ExecQueueMask &= ~(1 << i);
			
			Scheduler->ExecQueueMask |= 1 << i;
			
			TheirScheduler->ThreadsOnQueueCount--;
			Scheduler->ThreadsOnQueueCount++;
			
			LeftToSteal--;
		}
	}
}

PKTHREAD KiGetNextThread(bool MayDowngrade)
{
	KiAssertOwnDispatcherLock();
	PKSCHEDULER Scheduler = KiGetCurrentScheduler();
	PKTHREAD CurrentThread = Scheduler->CurrentThread;
	
	int MinPriority = 0;
	if (CurrentThread)
		MinPriority = CurrentThread->BasePriority;
	
	if (KiMoreAggressiveWorkStealing &&
		KepGetNextProcessorToStealWorkFrom() != KeGetCurrentPRCB()->Id)
	{
		// Steal many, many threads from this overcrowded processor.
		KepStealManyThreads();
	}
	
	// The "trick" behind this function is that we REALLY don't want to downgrade priority.
	
	PKTHREAD NextThread = NULL;
	
	// Try to run a slightly higher priority thread.
	NextThread = KepPopNextThreadIfNeeded(Scheduler, MinPriority + 1, false);
	if (NextThread)
		return NextThread;
	
	// No higher priority threads, try stealing another processor's
	NextThread = KepTryStealThread(MinPriority + 1);
	if (NextThread)
		return NextThread;
	
	// Nope, try same priority threads.
	NextThread = KepPopNextThreadIfNeeded(Scheduler, MinPriority, false);
	if (NextThread)
		return NextThread;
	
	// Nope, try stealing same priority threads from another processor.
	NextThread = KepTryStealThread(MinPriority);
	if (NextThread)
		return NextThread;
	
	if (!MayDowngrade)
		return NULL;
	
	// Nope, downgrade the priority.
	NextThread = KepTryStealThread(1);
	if (NextThread)
		return NextThread;
	
	// Finally, run our idle thread(s).
	NextThread = KepPopNextThreadIfNeeded(Scheduler, 0, false);
	if (NextThread)
		return NextThread;
	
	// No more threads to run!
	return NULL;
}

void KiPerformYield()
{
	KiAssertOwnDispatcherLock();
	
	PKSCHEDULER Scheduler = KiGetCurrentScheduler();
	PKTHREAD CurrentThread = Scheduler->CurrentThread;
	
	if (CurrentThread &&
		CurrentThread->BasePriority != PRIORITY_IDLE &&
		CurrentThread->TickScheduledAt)
	{
		AtFetchAdd(Scheduler->TicksSpentNonIdle, HalGetTickCount() - CurrentThread->TickScheduledAt);
		CurrentThread->TickScheduledAt = 0;
	}
	
	// If the current thread was "running" when it yielded, its quantum has
	// ended forcefully and we should place it back on the execution queue.
	if (!CurrentThread || CurrentThread->Status == KTHREAD_STATUS_RUNNING)
		return KiEndThreadQuantum();
	
	PKTHREAD NextThread = KiGetNextThread(true);
	if (!NextThread)
		KeCrash("KiPerformYield: Nothing to run");
	
	Scheduler->NextThread = NextThread;
	
	if (CurrentThread)
	{		
		// Was the thread terminated?
		if (CurrentThread->Status == KTHREAD_STATUS_TERMINATED)
		{
			// Yes. Issue a DPC (note, the last time DPCs were dispatched was
			// before this function was run, and the next time is after we've
			// switched away from this thread's stack) that will clean up after
			// this thread, by erasing its resources, such as its thread stack.
			
			KeInitializeDpc(&CurrentThread->DeleteDpc, KepCleanUpThread, CurrentThread);
			KeEnqueueDpc(&CurrentThread->DeleteDpc, NULL, NULL);
			
			ASSERT(NextThread != CurrentThread);
		}
	}
	
	Scheduler->QuantumUntil = 0;
}

bool KiNeedToSwitchThread()
{
	KiAssertOwnDispatcherLock();
	
	PKSCHEDULER Scheduler = KiGetCurrentScheduler();
	
	return Scheduler->NextThread != NULL;
}

// Get the process to switch the current address space to.
// It can either be the attached process, or the thread's process,
// if nothing is attached.
static PKPROCESS KepGetProcessToSwitchAddressSpaceTo(PKTHREAD Thread)
{
	return Thread->AttachedProcess ?
	       Thread->AttachedProcess :
	       Thread->Process;
}

void KiSwitchToNextThread()
{
	KiAssertOwnDispatcherLock();
	
	PKSCHEDULER Scheduler = KiGetCurrentScheduler();
	
	// Load other properties about the thread.
	if (!Scheduler->NextThread)
		KeCrash("KiSwitchToNextThread: Scheduler->NextThread is NULL, but we returned true for KiNeedToSwitchThread");
	
	SchedDebug("Scheduling In Thread %p", Scheduler->NextThread);
	
	PKTHREAD OldThread = Scheduler->CurrentThread;
	
	Scheduler->CurrentThread = Scheduler->NextThread;
	Scheduler->NextThread    = NULL;
	
	PKTHREAD Thread = Scheduler->CurrentThread;
	
	// Mark the thread as running.
	Thread->Status = KTHREAD_STATUS_RUNNING;
	
	// The time we weren't allowed to be stolen is up.
	Thread->DontSteal = false;
	
	// If there is a wait timer that we had set up, cancel it.
	KiCancelTimer(&Thread->WaitTimer);
	
	// Assign a quantum to the thread.
	KiAssignDefaultQuantum(Thread);
	
	Scheduler->QuantumUntil = Thread->QuantumUntil;
	
	Thread->TickScheduledAt = HalGetTickCount();
	
	// Switch to thread's process' address space.
	PKPROCESS DestProcess = KepGetProcessToSwitchAddressSpaceTo(Thread);
	
	if (MiGetCurrentPageMap() != DestProcess->PageMap)
		KiSwitchToAddressSpaceProcess(DestProcess);
	
	if (!IsListEmpty(&Thread->KernelApcQueue))
		KeIssueSoftwareInterrupt(IPL_APC);
	
	// Switch to thread's stack.
	uintptr_t StackBottom = (uintptr_t) Thread->Stack.Top + Thread->Stack.Size;
	KeGetCurrentPRCB()->SysCallStack = StackBottom;
	
#ifdef TARGET_AMD64
	// When an interrupt or exception happens and the CPL is ring 3,
	// this is fetched for a transition to ring 0.
	KeGetCurrentPRCB()->ArchData.Tss.RSP[0] = StackBottom;
	
	// Set the relevant MSRs.
	KeSetMSR(MSR_GS_BASE_KERNEL, (uint64_t) Thread->PebPointer);
	KeSetMSR(MSR_FS_BASE,        (uint64_t) Thread->TebPointer);
#endif
	
	if (OldThread == Thread)
	{
		// The old thread is the same as the new thread, there's no need to
		// do anything anymore.
		return;
	}
	
	// NOTE: There are two paths from here:
	// 1. Function execution will continue HERE but in the thread's context.
	//
	// 2. Function execution will continue in KiThreadEntryPoint.  This will unlock
	//    the dispatcher lock, lower IPL and jump to the thread's start routine.
	//
	//    It won't dispatch any APCs, but it's not necessary since the APC
	//    queue will be empty anyway.
	if (OldThread)
		KiSwitchThreadStack(&OldThread->StackPointer, &Thread->StackPointer);
	else
		KiSwitchThreadStackForever(Thread->StackPointer);
	
	// NOTE: Beyond this point, you CANNOT use "Thread" anymore to refer to the
	// thread that was just switched to.  You have to use "OldThread" - that was
	// the thread that was switched from when it saved its state in
	// KiSwitchThreadStack. But, well, there's nothing here for now, so fine.
}

void KiHandleQuantumEnd()
{
	KIPL Ipl = KiLockDispatcher();
	
	KiPerformYield();
	
	if (KiGetCurrentScheduler()->NextThread) {
		KiSwitchToNextThread();
	}
	
	KiUnlockDispatcher(Ipl);
}

void KeTimerTick()
{
	// Check if the current thread's quantum has expired. If it has, set
	// the PENDING_YIELD pending event and issue a software interrupt.
	
	// N.B. Don't mess with the dispatcher lock.
	// 1. KeTimerTick may have arrived at an inopportune time, by which
	//    I mean that it may have interrupted normal execution _while_
	//    the dispatcher is locked.
	// 2. You really don't need it!
	PKSCHEDULER Scheduler = KiGetCurrentScheduler();
	
	SchedDebug("Scheduler->QuantumUntil: %lld  HalGetTickCount: %lld", Scheduler->QuantumUntil, HalGetTickCount());
	
	// TODO: Optimize the amount of reschedules we perform.
	
	if (Scheduler->QuantumUntil <= HalGetTickCount())
	{
		// Thread's quantum has expired!!
		SchedDebug("Thread Quantum Has Expired");
		KiSetPendingQuantumEnd();
	}
	else if (HalUseOneShotIntTimer())
	{
		// We arrived a little too early! Reschedule in a bit
		HalRequestInterruptInTicks(100);
	}
	
	// If using a periodic timer, a new tick will just show up again soon.
}

// -------- Exposed API --------
void KeSetPriorityThread(PKTHREAD Thread, int Priority)
{
	KIPL OldIpl = KiLockDispatcher();
	KiSetPriorityThread(Thread, Priority);
	KiUnlockDispatcher(OldIpl);
}

void KeReadyThread(PKTHREAD Thread)
{
	KIPL OldIpl = KiLockDispatcher();
	KiReadyThread(Thread);
	KiUnlockDispatcher(OldIpl);
}

NO_RETURN void KeTerminateThread(KPRIORITY Increment)
{
	KIPL Ipl = KiLockDispatcher();
	
	PKTHREAD Thread = KeGetCurrentThread();
	
	// TODO: Release all mutexes this thread owns.
	
	KiCancelTimer(&Thread->WaitTimer);
	KiCancelTimer(&Thread->SleepTimer);
	
	// Mark the thread as terminated.
	Thread->Status = KTHREAD_STATUS_TERMINATED;
	
	Thread->IncrementTerminated = Increment;
	
	// Unlock the dispatcher and request an end to current quantum.
	KiUnlockDispatcher(IPL_DPC);
	KiHandleQuantumEnd(Ipl);
	
	KeCrash("KeTerminateThread: After yielding, terminated thread was scheduled back in");
}
