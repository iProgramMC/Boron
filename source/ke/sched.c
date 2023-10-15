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
#include <ke.h>
#include <hal.h>

//#define SCHED_DEBUG
#ifdef SCHED_DEBUG
#define DbgPrint(...) SLogMsg(__VA_ARGS__)
#define Dbg2Print(...) SLogMsg(__VA_ARGS__)
#else
#define DbgPrint(...)
#define Dbg2Print(...) SLogMsg(__VA_ARGS__)
#endif

LIST_ENTRY KiGlobalThreadList;

PKSCHEDULER KeGetCurrentScheduler()
{
	return &KeGetCurrentPRCB()->Scheduler;
}

PKTHREAD KeGetCurrentThread()
{
	return KeGetCurrentScheduler()->CurrentThread;
}

void KeSchedulerInitUP()
{
	InitializeListHead(&KiGlobalThreadList);
}

void KeSetPriorityThread(PKTHREAD Thread, int Priority)
{
	if (!IsListEmpty(&Thread->EntryQueue))
	{
		PKSCHEDULER Scheduler = KeGetCurrentScheduler();
		
		// Remove from the old place in its queue, and assign it to the new place in the queue.
		RemoveEntryList(&Thread->EntryQueue);
		
		if (Thread->Priority > Priority)
			InsertHeadList(&Scheduler->ExecQueue[Priority], &Thread->EntryQueue);
		else
			InsertTailList(&Scheduler->ExecQueue[Priority], &Thread->EntryQueue);
	}
	
	Thread->Priority = Priority;
}

static NO_RETURN void KiIdleThreadEntry(UNUSED void* Context)
{
	LogMsg("Hello from the idle thread from CPU %d!", KeGetCurrentPRCB()->LapicId);
	
	while (true)
		KeWaitForNextInterrupt();
}

static NO_RETURN void KiTestThread1Entry(UNUSED void* Context)
{
	while (true)
	{
		LogMsg("Hello from the test1 thread from CPU %d!  %lld", KeGetCurrentPRCB()->LapicId, HalGetTickCount());
		KeWaitForNextInterrupt();
	}
}

static NO_RETURN void KiTestThread2Entry(UNUSED void* Context)
{
	while (true)
	{
		LogMsg("Hello from the test2 thread from CPU %d!  %lld", KeGetCurrentPRCB()->LapicId, HalGetTickCount());
		KeWaitForNextInterrupt();
	}
}

void KeReadyThread(PKTHREAD Thread)
{
	Thread->Status = KTHREAD_STATUS_READY;
	
	KIPL OldIpl = KeRaiseIPL(IPL_DPC);
	PKSCHEDULER Scheduler = KeGetCurrentScheduler();
	
	InsertTailList(&KiGlobalThreadList,                     &Thread->EntryGlobal);
	InsertTailList(&Scheduler->ThreadList,                  &Thread->EntryList);
	InsertTailList(&Scheduler->ExecQueue[Thread->Priority], &Thread->EntryQueue);
	
	KeLowerIPL(OldIpl);
}

static void KepCreateTestThread(PKTHREAD_START Start)
{
	PKTHREAD Thread = KeCreateEmptyThread();
	KeInitializeThread(Thread, EX_NO_MEMORY_HANDLE, Start, NULL);
	KeSetPriorityThread(Thread, PRIORITY_NORMAL);
	KeReadyThread(Thread);
}

void KeSchedulerInit()
{
	KIPL OldIpl = KeRaiseIPL(IPL_DPC);
	
	PKSCHEDULER Scheduler = KeGetCurrentScheduler();
	
	InitializeListHead(&Scheduler->ThreadList);
	
	for (int i = 0; i < PRIORITY_COUNT; i++)
		InitializeListHead(&Scheduler->ExecQueue[i]);
	
	Scheduler->CurrentThread = NULL;
	
	// Create an idle thread.
	PKTHREAD Thread = KeCreateEmptyThread();
	KeInitializeThread(Thread, EX_NO_MEMORY_HANDLE, KiIdleThreadEntry, NULL);
	KeSetPriorityThread(Thread, PRIORITY_IDLE);
	KeReadyThread(Thread);
	
	// Create two testing threads.
	KepCreateTestThread(KiTestThread1Entry);
	KepCreateTestThread(KiTestThread2Entry);
	
	KeLowerIPL(OldIpl);
}

// Commit to running threads managed by the scheduler.
NO_RETURN void KeSchedulerCommit()
{
	KeSetPendingEvent(PENDING_QUANTUM_END);
	KeIssueSoftwareInterrupt();
	
	// Wait for the waves to pick us up...
	while (true)
		KeWaitForNextInterrupt();
}

static PKTHREAD KepPopNextThreadIfNeeded(PKSCHEDULER Sched, int MinPriority)
{
	for (int Priority = PRIORITY_COUNT - 1; Priority >= MinPriority; Priority--)
	{
		// Is the list empty?
		if (IsListEmpty(&Sched->ExecQueue[Priority]))
			continue;
		
		// Pop off the thread.
		return CONTAINING_RECORD(RemoveHeadList(&Sched->ExecQueue[Priority]), KTHREAD, EntryQueue);
	}
	
	// No thread with at least MinPriority priority was picked.
	return NULL;
}

void KiAssignDefaultQuantum(PKTHREAD Thread)
{
	uint64_t QuantumTicks = HalGetTicksPerSecond() * MAX_QUANTUM_US / 1000000;
	Thread->QuantumUntil = HalGetTickCount() + QuantumTicks;
	
	if (HalUseOneShotTimer())
	{
		HalRequestInterruptInTicks(HalGetItTicksPerSecond() * MAX_QUANTUM_US / 1000000);
	}
	else
	{
		// It's just going to show up
	}
}

void KiEndThreadQuantum(PKREGISTERS Regs)
{
	// NOTE: This function is called only if the thread's quantum expired.
	// Thus, there's always going to be something running at at least that thread's
	// priority, including the current thread itself.
	
	PKSCHEDULER Scheduler = KeGetCurrentScheduler();
	PKTHREAD CurrentThread = Scheduler->CurrentThread;
	
	int MinPriority = 0;
	if (CurrentThread)
		MinPriority = CurrentThread->Priority;
	
	PKTHREAD NextThread = KepPopNextThreadIfNeeded(Scheduler, MinPriority);
	if (!NextThread)
	{
		// If we don't have any more threads that have at least the priority
		// of the current one, return immediately. We are not going to switch.
		KiAssignDefaultQuantum(CurrentThread);
		return;
	}
	
	Scheduler->NextThread = NextThread;
	
	// Since the current thread's quantum has expired, it's of course ready to run again!
	if (CurrentThread)
	{
		CurrentThread->Status = KTHREAD_STATUS_READY;
		
		DbgPrint("Emplacing Thread %p In Queue", CurrentThread);
		
		// Emplace it back on the ready queue.
		InsertTailList(&Scheduler->ExecQueue[CurrentThread->Priority], &CurrentThread->EntryQueue);
		
		// Save the registers
		CurrentThread->Registers = *Regs;
	}
	
	// Unload the current thread.
	Scheduler->CurrentThread = NULL;
}

bool KiNeedToSwitchThread()
{
	PKSCHEDULER Scheduler = KeGetCurrentScheduler();
	
	return Scheduler->NextThread != NULL;
}

PKREGISTERS KiSwitchToNextThread()
{
	PKSCHEDULER Scheduler = KeGetCurrentScheduler();
	
	// Load other properties about the thread.
	if (!Scheduler->NextThread)
		KeCrash("KiSwitchToNextThread: Scheduler->NextThread is NULL, but we returned true for KiNeedToSwitchThread");
	
	// There is no current thread loaded.
	// If KiEndThreadQuantum has picked a next thread, it's also gotten rid
	// of CurrentThread.
	DbgPrint("Scheduling In Thread %p", Scheduler->NextThread);
	
	Scheduler->CurrentThread = Scheduler->NextThread;
	Scheduler->NextThread    = NULL;
	
	PKTHREAD Thread = Scheduler->CurrentThread;
	
	// Mark the thread as running.
	Thread->Status = KTHREAD_STATUS_RUNNING;
	
	// Assign a quantum to the thread.
	KiAssignDefaultQuantum(Thread);
	
	return &Thread->Registers;
}

void KiRescheduleTimerNoChange()
{
	if (!HalUseOneShotTimer())
		return; // it's going to trigger again anyways
	
	PKTHREAD Thread = KeGetCurrentThread();
	
	int64_t TicksTillQuantumExpiry = Thread->QuantumUntil - HalGetTickCount();
	if (TicksTillQuantumExpiry < 500)
	{
		// Do it anyway
		DbgPrint("Thread Quantum Has Almost Expired");
		KeSetPendingEvent(PENDING_QUANTUM_END);
		KeIssueSoftwareInterrupt(); // This interrupt will show up when we exit this interrupt.
		return;
	}
	
	int64_t NanosecondsTillQuantumExpiry = TicksTillQuantumExpiry * 1000000000 / HalGetTicksPerSecond();
	int64_t ItTicksTillQuantumExpiry = HalGetItTicksPerSecond() * NanosecondsTillQuantumExpiry / 1000000000;
	
	DbgPrint("Correction: IT: %lld NS: %lld TI: %lld",
	         ItTicksTillQuantumExpiry,
			 NanosecondsTillQuantumExpiry,
			 TicksTillQuantumExpiry);
	
	if (ItTicksTillQuantumExpiry == 0)
	{
		// Do it anyway
		DbgPrint("ItTicksTillQuantumExpiry Is Zero, Marking Quantum End Anyway");
		KeSetPendingEvent(PENDING_QUANTUM_END);
		KeIssueSoftwareInterrupt(); // This interrupt will show up when we exit this interrupt.
		return;
	}
	
	HalRequestInterruptInTicks(ItTicksTillQuantumExpiry);
}

void KeTimerTick()
{
	// Check if the current thread's quantum has expired.
	PKTHREAD Thread = KeGetCurrentThread();
	
	if (!Thread)
	{
		// TODO
		return;
	}
	
	DbgPrint("Thread->QuantumUntil: %lld  HalGetTickCount: %lld", Thread->QuantumUntil, HalGetTickCount());
	
	if (Thread->QuantumUntil <= HalGetTickCount())
	{
		// Thread's quantum has expired!!
		DbgPrint("Thread Quantum Has Expired");
		KeSetPendingEvent(PENDING_QUANTUM_END);
		KeIssueSoftwareInterrupt(); // This interrupt will show up when we exit this interrupt.
	}
	else
	{
		KiRescheduleTimerNoChange();
	}
}
