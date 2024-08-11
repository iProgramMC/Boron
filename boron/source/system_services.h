// Temporary list of currently implemented system services.
// This will be removed once the system service dispatch table is implemented.


BSTATUS OSReleaseMutex(HANDLE);
BSTATUS OSQueryMutex(HANDLE, int*);
BSTATUS OSCreateMutex(PHANDLE, POBJECT_ATTRIBUTES);
BSTATUS OSOpenMutex(PHANDLE, POBJECT_ATTRIBUTES, int);


