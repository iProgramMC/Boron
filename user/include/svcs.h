#pragma once

// NOTE: Do not include bare! Always include "boron.h".

#ifdef __cplusplus
extern "C" {
#endif

BSTATUS OSAllocateVirtualMemory(
	HANDLE ProcessHandle,
	void** BaseAddressInOut,
	size_t* RegionSizeInOut,
	int AllocationType,
	int Protection
);

BSTATUS OSClose(HANDLE Handle);

BSTATUS OSCreateEvent(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes, int EventType, bool State);

BSTATUS OSCreateMutex(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes);

#ifdef IS_BORON_DLL

BSTATUS OSCreateProcessInternal(
	PHANDLE OutHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	HANDLE ParentProcessHandle,
	bool InheritHandles
);

#endif

BSTATUS OSCreateThread(
	PHANDLE OutHandle,
	HANDLE ProcessHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PKTHREAD_START ThreadStart,
	void* ThreadContext,
	bool CreateSuspended
);

BSTATUS OSDuplicateHandle(HANDLE SourceHandle, HANDLE DestinationProcessHandle, PHANDLE OutNewHandle, int OpenFlags);

NO_RETURN void OSExitProcess(int ExitCode);

NO_RETURN void OSExitThread();

BSTATUS OSFreeVirtualMemory(
	HANDLE ProcessHandle,
	void* BaseAddress,
	size_t RegionSize,
	int FreeType
);

BSTATUS OSGetAlignmentFile(HANDLE Handle, size_t* AlignmentOut);

void* OSGetCurrentTeb();

BSTATUS OSGetLengthFile(HANDLE FileHandle, uint64_t* Length);

BSTATUS OSGetMappedFileHandle(PHANDLE OutHandle, HANDLE ProcessHandle, uintptr_t Address);

BSTATUS OSMapViewOfObject(
	HANDLE ProcessHandle,
	HANDLE MappedObject,
	void** BaseAddressInOut,
	size_t ViewSize,
	int AllocationType,
	uint64_t SectionOffset,
	int Protection
);

BSTATUS OSOpenEvent(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes);

BSTATUS OSOpenFile(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes);

BSTATUS OSOpenMutex(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes);

BSTATUS OSOutputDebugString(const char* String, size_t StringLength);

BSTATUS OSPulseEvent(HANDLE EventHandle);

BSTATUS OSQueryEvent(HANDLE EventHandle, int* EventState);

BSTATUS OSQueryMutex(HANDLE MutexHandle, int* MutexState);

BSTATUS OSReadDirectoryEntries(PIO_STATUS_BLOCK Iosb, HANDLE FileHandle, size_t DirectoryEntryCount, PIO_DIRECTORY_ENTRY DirectoryEntries);

BSTATUS OSReadFile(PIO_STATUS_BLOCK Iosb, HANDLE Handle, uint64_t ByteOffset, void* Buffer, size_t Length, uint32_t Flags);

BSTATUS OSReleaseMutex(HANDLE MutexHandle);

BSTATUS OSResetDirectoryReadHead(HANDLE FileHandle);

BSTATUS OSResetEvent(HANDLE EventHandle);

BSTATUS OSSetCurrentTeb(void* Ptr);

BSTATUS OSSetEvent(HANDLE EventHandle);

BSTATUS OSSetSuspendedThread(HANDLE ThreadHandle, bool IsSuspended);

BSTATUS OSSleep(int Milliseconds);

BSTATUS OSTerminateThread(HANDLE ThreadHandle);

BSTATUS OSTouchFile(HANDLE Handle, bool IsWrite);

BSTATUS OSWaitForMultipleObjects(
	int ObjectCount,
	PHANDLE ObjectsArray,
	int WaitType,
	bool Alertable,
	int TimeoutMS
);

BSTATUS OSWaitForSingleObject(HANDLE Handle, bool Alertable, int TimeoutMS);

BSTATUS OSWriteFile(PIO_STATUS_BLOCK Iosb, HANDLE Handle, uint64_t ByteOffset, const void* Buffer, size_t Length, uint32_t Flags, uint64_t* OutSize);

BSTATUS OSWriteVirtualMemory(HANDLE ProcessHandle, void* TargetAddress, const void* Source, size_t ByteCount);

#ifdef __cplusplus
}
#endif
