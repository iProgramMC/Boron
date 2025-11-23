
CALL 0,  5, OSAllocateVirtualMemory
CALL 1,  1, OSClose
CALL 2,  4, OSCreateEvent
CALL 3,  2, OSCreateMutex
CALL 4,  4, OSCreatePipe
CALL 5,  4, OSCreateProcessInternal
CALL 6,  3, OSCreateTerminal
CALL 7,  3, OSCreateTerminalIoHandles
CALL 8,  6, OSCreateThread
CALL 9,  6, OSDeviceIoControl
CALL 10, 4, OSDuplicateHandle
CALL 11, 1, OSExitProcess
CALL 12, 0, OSExitThread
CALL 13, 4, OSFreeVirtualMemory
CALL 14, 2, OSGetAlignmentFile
CALL 15, 0, OSGetCurrentPeb
CALL 16, 0, OSGetCurrentTeb
CALL 17, 2, OSGetExitCodeProcess
CALL 18, 2, OSGetLengthFile
CALL 19, 3, OSGetMappedFileHandle
CALL 20, 1, OSGetTickCount
CALL 21, 1, OSGetTickFrequency
CALL 22, 1, OSGetVersionNumber
//   23     OSMapViewOfObject
CALL 24, 2, OSOpenEvent
CALL 25, 2, OSOpenFile
CALL 26, 2, OSOpenMutex
CALL 27, 2, OSOutputDebugString
CALL 28, 1, OSPulseEvent
CALL 29, 2, OSQueryEvent
CALL 30, 2, OSQueryMutex
CALL 31, 4, OSReadDirectoryEntries
//   32     OSReadFile
CALL 33, 4, OSReadVirtualMemory
CALL 34, 1, OSReleaseMutex
CALL 35, 1, OSResetEvent
//   36     OSSeekFile
CALL 37, 1, OSSetCurrentPeb
CALL 38, 1, OSSetCurrentTeb
CALL 39, 1, OSSetEvent
CALL 30, 1, OSSetExitCode
CALL 41, 2, OSSetPebProcess
CALL 42, 2, OSSetSuspendedThread
CALL 43, 1, OSSleep
CALL 44, 1, OSTerminateThread
CALL 45, 1, OSTouchFile
CALL 46, 5, OSWaitForMultipleObjects
CALL 47, 3, OSWaitForSingleObject
//   48     OSWriteFile
CALL 49, 4, OSWriteVirtualMemory

#ifdef IS_64_BIT
CALL 23, 7, OSMapViewOfObject
CALL 32, 6, OSReadFile
CALL 36, 4, OSSeekFile
CALL 48, 7, OSWriteFile
#else
CALL 23, 8, OSMapViewOfObject
CALL 32, 7, OSReadFile
CALL 36, 5, OSSeekFile
CALL 48, 8, OSWriteFile
#endif
