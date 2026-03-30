/***
	The Boron Operating System
	Copyright (C) 2025-2026 iProgramInCpp

Module name:
	ke/arm/syscall.c
	
Abstract:
	This module implements the system service dispatcher
	for the arm architecture.
	
Author:
	iProgramInCpp - 30 December 2025
***/
#include "../ki.h"

#include <ke.h>
#include <ex.h>
#include <ob.h>
#include <ps.h>
#include <io.h>
#include <tty.h>

//#define ENABLE_SYSCALL_TRACE

static BSTATUS OSMapViewOfObject_Call(
	HANDLE ProcessHandle,
	HANDLE MappedObject,
	void** BaseAddressInOut,
	size_t ViewSize,
	int AllocationType,
	UNUSED int DummyPadding,
	uint64_t SectionOffset,
	int Protection
)
{
	return OSMapViewOfObject(
		ProcessHandle,
		MappedObject,
		BaseAddressInOut,
		ViewSize,
		AllocationType,
		SectionOffset,
		Protection
	);
}

static BSTATUS OSSeekFile_Call(
	HANDLE FileHandle,
	UNUSED int DummyPadding,
	int64_t Offset,
	int Whence,
	uint64_t* NewOutOffset
)
{
	return OSSeekFile(
		FileHandle,
		Offset,
		Whence,
		NewOutOffset
	);
}

const void* const KiSystemServiceTable[] =
{
	OSAllocateVirtualMemory,
	OSCheckIsTerminalFile,
	OSClose,
	OSCreateEvent,
	OSCreateMutex,
	OSCreatePipe,
	OSCreateProcess,
	OSCreateTerminal,
	OSCreateTerminalIoHandles,
	OSCreateThread,
	OSDeviceIoControl,
	OSDuplicateHandle,
	OSExitProcess,
	OSExitThread,
	OSFreeVirtualMemory,
	OSGetAlignmentFile,
	OSGetCurrentPeb,
	OSGetCurrentTeb,
	OSGetExitCodeProcess,
	OSGetLengthFile,
	OSGetMappedFileHandle,
	OSGetTickCount,
	OSGetTickFrequency,
	OSGetVersionNumber,
	OSMapViewOfObject_Call,
	OSOpenEvent,
	OSOpenFile,
	OSOpenMutex,
	OSOutputDebugString,
	OSPulseEvent,
	OSQueryEvent,
	OSQueryMutex,
	OSReadDirectoryEntries,
	OSReadFile,
	OSReadVirtualMemory,
	OSReleaseMutex,
	OSResetEvent,
	OSSeekFile_Call,
	OSSetCurrentPeb,
	OSSetCurrentTeb,
	OSSetEvent,
	OSSetExitCode,
	OSSetPebProcess,
	OSSetSuspendedThread,
	OSSleep,
	OSTerminateThread,
	OSTouchFile,
	OSWaitForMultipleObjects,
	OSWaitForSingleObject,
	OSWriteFile,
	OSWriteVirtualMemory,
	OSForkProcess,
	OSQueryVirtualMemoryInformation,
	OSCloseAllUninheritableHandles,
	OSCheckIsValidHandle,
	OSCreateFile,
	OSCreateDirectory,
	OSCreateSymbolicLink,
	OSSetImageNameProcess,
	OSQuerySystemInformation,
	OSShutDownSystem,
};

#define KI_SYSCALL_COUNT ARRAY_COUNT(KiSystemServiceTable)

typedef int(*KI_SYSCALL_HANDLER)(
	uintptr_t p1,
	uintptr_t p2,
	uintptr_t p3,
	uintptr_t p4,
	uintptr_t p5,
	uintptr_t p6,
	uintptr_t p7,
	uintptr_t p8,
	uintptr_t p9,
	uintptr_t ip,
	uintptr_t sp
);

extern void KePrintSystemServiceDebug(size_t Call); // cpu.c

void KiCheckTerminatedUserMode()
{
	// Before entering user mode, check if the thread was terminated first.
	if (KeGetCurrentThread()->PendingTermination)
		KiTerminateUserModeThread(KeGetCurrentThread()->IncrementTerminated);
}

void KiSystemServiceHandler(PKREGISTERS Regs, uintptr_t Sp_Usr)
{
	uintptr_t CallNumber = 0;
	uintptr_t Opcode;
	BSTATUS Status = MmSafeCopy(&Opcode, (void*)(Regs->Lr - 4), sizeof(uintptr_t), MODE_USER, false);
	if (FAILED(Status))
	{
		Regs->R0 = Status;
		return;
	}
	
	// Keep Immediate (the system call will be executed like `svc #callNumber`)
	Opcode &= 0xFFFFFF;
	
	// This is the call number
	CallNumber = Opcode;
	
	if (CallNumber >= KI_SYSCALL_COUNT)
	{
		Regs->R0 = STATUS_INVALID_PARAMETER;
		return;
	}
	
#ifdef ENABLE_SYSCALL_TRACE
	KePrintSystemServiceDebug(CallNumber);
#endif
	
	// Now call the function.
	//
	// Note: Most system call handlers completely ignore the 7th and 8th parameters, except for
	// the OSForkProcess handler.
	KI_SYSCALL_HANDLER Handler = KiSystemServiceTable[CallNumber];
	int Return = Handler(
		Regs->R0,
		Regs->R1,
		Regs->R2,
		Regs->R3,
		Regs->R4,
		Regs->R5,
		Regs->R6,
		Regs->R7,
		Regs->R8,
		Regs->Lr,
		Sp_Usr
	);
	Regs->R0 = Return;
	
	KiCheckTerminatedUserMode();
}

#ifdef ENABLE_SYSCALL_TRACE

void KePrintSystemServiceDebug(size_t Syscall)
{
	// Format: "[ThreadPointer] - Syscall [Number] (FunctionPointer) ([FunctionName])"
	const void* Handler = KiSystemServiceTable[Syscall];
	const char* FunctionName = DbgLookUpRoutineNameByAddressExact((uintptr_t) Handler);
	DbgPrint("SYSCALL: %p - %d %p %s", KeGetCurrentThread(), (int) Syscall, Handler, FunctionName);
}

#endif
