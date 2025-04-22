#include <boron.h>

BSTATUS OSCreateThread(
	PHANDLE OutHandle,
	HANDLE ProcessHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PKTHREAD_START ThreadStart,
	void* ThreadContext,
	bool CreateSuspended
);

void OSExitThread();
void OSDummy();

void DLLEntryPoint()
{
	OSDummy();
	OSDummy();
	
	OSExitThread();
}
