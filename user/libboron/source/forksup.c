#include <boron.h>

HIDDEN
void OSDLLForkEntry();

BSTATUS OSForkProcess(PHANDLE OutChildProcessHandle)
{
	// Most of the fork procedure is implemented in the kernel, but some
	// userspace support is still required.
	BSTATUS Status = OSForkProcessInternal(OutChildProcessHandle);
	if (Status == STATUS_IS_CHILD_PROCESS) {
		OSDLLForkEntry();
	}
	
	return Status;
}

HIDDEN
void OSDLLForkEntry()
{
	DbgPrint("OSDLLForkEntry: entered");
	
	// TODO: create new critical section events
}
