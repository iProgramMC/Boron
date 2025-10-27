
CALL 0,  5, OSAllocateVirtualMemory
CALL 1,  1, OSClose
CALL 2,  4, OSCreateEvent
CALL 3,  2, OSCreateMutex
CALL 4,  4, OSCreatePipe
CALL 5,  4, OSCreateProcessInternal
CALL 6,  6, OSCreateThread
CALL 7,  6, OSDeviceIoControl
CALL 8,  4, OSDuplicateHandle
CALL 9,  1, OSExitProcess
CALL 10, 0, OSExitThread
CALL 11, 4, OSFreeVirtualMemory
CALL 12, 2, OSGetAlignmentFile
CALL 13, 0, OSGetCurrentPeb
CALL 14, 0, OSGetCurrentTeb
CALL 15, 2, OSGetExitCodeProcess
CALL 16, 2, OSGetLengthFile
CALL 17, 3, OSGetMappedFileHandle
CALL 18, 1, OSGetTickCount
CALL 19, 1, OSGetTickFrequency
CALL 20, 1, OSGetVersionNumber
//   21     OSMapViewOfObject
CALL 22, 2, OSOpenEvent
CALL 23, 2, OSOpenFile
CALL 24, 2, OSOpenMutex
CALL 25, 2, OSOutputDebugString
CALL 26, 1, OSPulseEvent
CALL 27, 2, OSQueryEvent
CALL 28, 2, OSQueryMutex
CALL 29, 4, OSReadDirectoryEntries
//   30     OSReadFile
CALL 31, 1, OSReleaseMutex
CALL 32, 1, OSResetDirectoryReadHead
CALL 33, 1, OSResetEvent
CALL 34, 1, OSSetCurrentPeb
CALL 35, 1, OSSetCurrentTeb
CALL 36, 1, OSSetEvent
CALL 37, 1, OSSetExitCode
CALL 38, 2, OSSetPebProcess
CALL 39, 2, OSSetSuspendedThread
CALL 40, 1, OSSleep
CALL 41, 1, OSTerminateThread
CALL 42, 1, OSTouchFile
CALL 43, 5, OSWaitForMultipleObjects
CALL 44, 3, OSWaitForSingleObject
//   45     OSWriteFile
CALL 46, 4, OSWriteVirtualMemory

#ifdef IS_64_BIT
CALL 21, 7, OSMapViewOfObject
CALL 30, 6, OSReadFile
CALL 45, 7, OSWriteFile
#else
CALL 21, 8, OSMapViewOfObject
CALL 30, 7, OSReadFile
CALL 45, 8, OSWriteFile
#endif
