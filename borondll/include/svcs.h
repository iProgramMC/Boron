#pragma once

//TODO: NO_RETURN void OSExitProcess();

NO_RETURN void OSExitThread();

BSTATUS OSClose(HANDLE Handle);

BSTATUS OSOutputDebugString(const char* String, size_t StringLength);

BSTATUS OSCreateThread(
	PHANDLE OutHandle,
	HANDLE ProcessHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PKTHREAD_START ThreadStart,
	void* ThreadContext,
	bool CreateSuspended
);

NO_RETURN void OSExitThread();

void OSDummy();

BSTATUS OSClose(HANDLE Handle);

void OSWaitForSingleObject(HANDLE Handle, bool Alertable, int TimeoutMS);
