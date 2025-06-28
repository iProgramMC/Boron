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
