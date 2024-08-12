// Temporary list of currently implemented system services.
// This will be removed once the system service dispatch table is implemented.

BSTATUS OSClose(HANDLE);
BSTATUS OSWaitForSingleObject(HANDLE, bool, int);
BSTATUS OSWaitForMultipleObjects(int, PHANDLE, int, bool, int);
BSTATUS OSCreateMutex(PHANDLE, POBJECT_ATTRIBUTES);
BSTATUS OSOpenMutex(PHANDLE, POBJECT_ATTRIBUTES, int);
BSTATUS OSReleaseMutex(HANDLE);
BSTATUS OSQueryMutex(HANDLE, int*);
BSTATUS OSCreateEvent(PHANDLE, POBJECT_ATTRIBUTES, int, bool);
BSTATUS OSOpenEvent(PHANDLE, POBJECT_ATTRIBUTES, int);
BSTATUS OSSetEvent(HANDLE);
BSTATUS OSResetEvent(HANDLE);
BSTATUS OSPulseEvent(HANDLE);
BSTATUS OSQueryEvent(HANDLE, int*);


