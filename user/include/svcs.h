#pragma once

BSTATUS OSClose(HANDLE Handle);

BSTATUS OSCreateThread(
	PHANDLE OutHandle,
	HANDLE ProcessHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PKTHREAD_START ThreadStart,
	void* ThreadContext,
	bool CreateSuspended
);

BSTATUS OSCreateProcess(
	PHANDLE OutHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	HANDLE ParentProcessHandle,
	bool InheritHandles
);

void OSDummy();

NO_RETURN void OSExitThread();

BSTATUS OSReadFile(PIO_STATUS_BLOCK Iosb, HANDLE Handle, uint64_t ByteOffset, void* Buffer, size_t Length, uint32_t Flags);

//TODO: BSTATUS OSWriteFile(PIO_STATUS_BLOCK Iosb, HANDLE Handle, uint64_t ByteOffset, const void* Buffer, size_t Length, uint32_t Flags, uint64_t* OutSize);

BSTATUS OSOpenFile(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes);

BSTATUS OSGetLengthFile(HANDLE FileHandle, uint64_t* Length);

BSTATUS OSTouchFile(HANDLE Handle, bool IsWrite);

BSTATUS OSGetAlignmentFile(HANDLE Handle, size_t* AlignmentOut);

BSTATUS OSAllocateVirtualMemory(
	HANDLE ProcessHandle,
	void** BaseAddressInOut,
	size_t* RegionSizeInOut,
	int AllocationType,
	int Protection
);

BSTATUS OSFreeVirtualMemory(
	HANDLE ProcessHandle,
	void* BaseAddress,
	size_t RegionSize,
	int FreeType
);

BSTATUS OSMapViewOfObject(
	HANDLE ProcessHandle,
	HANDLE MappedObject,
	void** BaseAddressInOut,
	size_t ViewSize,
	int AllocationType,
	uint64_t SectionOffset,
	int Protection
);

//TODO: NO_RETURN void OSExitProcess();

BSTATUS OSOutputDebugString(const char* String, size_t StringLength);

void OSWaitForSingleObject(HANDLE Handle, bool Alertable, int TimeoutMS);

BSTATUS OSWaitForMultipleObjects(
	int ObjectCount,
	PHANDLE ObjectsArray,
	int WaitType,
	bool Alertable,
	int TimeoutMS
);
