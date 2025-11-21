/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/i386/syscall.c
	
Abstract:
	This module implements the system service dispatcher
	for the i386 architecture.
	
Author:
	iProgramInCpp - 22 October 2025
***/
#include "../ki.h"

#include <ke.h>
#include <ex.h>
#include <ob.h>
#include <ps.h>
#include <io.h>
#include <tty.h>

//#define ENABLE_SYSCALL_TRACE

typedef struct
{
	uint64_t SectionOffset;
	int Protection;
}
MAP_VIEW_OF_OBJECT_PARAMS, *PMAP_VIEW_OF_OBJECT_PARAMS;

typedef struct
{
	size_t Length;
	uint32_t Flags;
}
READ_FILE_PARAMS, *PREAD_FILE_PARAMS;

typedef struct
{
	size_t Length;
	uint32_t Flags;
	uint64_t* OutSize;
}
WRITE_FILE_PARAMS, *PWRITE_FILE_PARAMS;

BSTATUS OSMapViewOfObject_Call(
	HANDLE ProcessHandle,
	HANDLE MappedObject,
	void** BaseAddressInOut,
	size_t ViewSize,
	int AllocationType,
	PMAP_VIEW_OF_OBJECT_PARAMS ExtraParamsPtr
)
{
	BSTATUS Status;
	MAP_VIEW_OF_OBJECT_PARAMS ExtraParams;
	
	Status = MmSafeCopy(&ExtraParams, ExtraParamsPtr, sizeof(MAP_VIEW_OF_OBJECT_PARAMS), MODE_USER, false);
	if (FAILED(Status))
		return Status;
	
	return OSMapViewOfObject(
		ProcessHandle,
		MappedObject,
		BaseAddressInOut,
		ViewSize,
		AllocationType,
		ExtraParams.SectionOffset,
		ExtraParams.Protection
	);
}

BSTATUS OSReadFile_Call(
	PIO_STATUS_BLOCK Iosb,
	HANDLE Handle,
	uint64_t ByteOffset, // 2 parameters
	void* Buffer,
	PREAD_FILE_PARAMS ExtraParamsPtr
)
{
	BSTATUS Status;
	READ_FILE_PARAMS ExtraParams;
	
	Status = MmSafeCopy(&ExtraParams, ExtraParamsPtr, sizeof(READ_FILE_PARAMS), MODE_USER, false);
	if (FAILED(Status))
		return Status;
	
	return OSReadFile(Iosb, Handle, ByteOffset, Buffer, ExtraParams.Length, ExtraParams.Flags);
}

BSTATUS OSWriteFile_Call(
	PIO_STATUS_BLOCK Iosb,
	HANDLE Handle,
	uint64_t ByteOffset, // 2 parameters
	const void* Buffer,
	PWRITE_FILE_PARAMS ExtraParamsPtr
)
{
	BSTATUS Status;
	WRITE_FILE_PARAMS ExtraParams;
	
	Status = MmSafeCopy(&ExtraParams, ExtraParamsPtr, sizeof(WRITE_FILE_PARAMS), MODE_USER, false);
	if (FAILED(Status))
		return Status;
	
	return OSWriteFile(Iosb, Handle, ByteOffset, Buffer, ExtraParams.Length, ExtraParams.Flags, ExtraParams.OutSize);
}

// N.B. extra arguments will be ignored by the called function.
typedef int(*KI_SYSCALL_HANDLER)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

const void* const KiSystemServiceTable[] =
{
	OSAllocateVirtualMemory,
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
	OSReadFile_Call,
	OSReleaseMutex,
	OSResetEvent,
	OSSeekFile,
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
	OSWriteFile_Call,
	OSWriteVirtualMemory,
};

#define KI_SYSCALL_COUNT ARRAY_COUNT(KiSystemServiceTable)

extern void KePrintSystemServiceDebug(size_t Call); // cpu.c
extern void KiCheckTerminatedUserMode(); // traps.c

PKREGISTERS KiSystemServiceHandler(PKREGISTERS Regs)
{
	if (Regs->Eax >= KI_SYSCALL_COUNT)
	{
		Regs->Eax = STATUS_INVALID_PARAMETER;
		return Regs;
	}
	
#ifdef ENABLE_SYSCALL_TRACE
	KePrintSystemServiceDebug(Regs->Eax);
#endif
	
	// Now call the function.
	KI_SYSCALL_HANDLER Handler = KiSystemServiceTable[Regs->Eax];
	int Return = Handler(Regs->Ebx, Regs->Ecx, Regs->Edx, Regs->Esi, Regs->Edi, Regs->Ebp);
	Regs->Eax = Return;
	
	KiCheckTerminatedUserMode();
	
	return Regs;
}
