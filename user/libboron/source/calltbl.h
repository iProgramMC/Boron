// Num  - System Call Number
// AC   - Argument Count
// Name - System Call Export Name
//
//   Num AC Name
CALL 0,  5, OSAllocateVirtualMemory
CALL 1,  1, OSCheckIsTerminalFile
CALL 2,  1, OSClose
CALL 3,  4, OSCreateEvent
CALL 4,  2, OSCreateMutex
CALL 5,  4, OSCreatePipe
CALL 6,  5, OSCreateProcessInternal
CALL 7,  3, OSCreateTerminal
CALL 8,  3, OSCreateTerminalIoHandles
CALL 9,  6, OSCreateThread
CALL 10, 6, OSDeviceIoControl
CALL 11, 4, OSDuplicateHandle
CALL 12, 1, OSExitProcess
CALL 13, 0, OSExitThread
CALL 14, 4, OSFreeVirtualMemory
CALL 15, 2, OSGetAlignmentFile
CALL 16, 0, OSGetCurrentPeb
CALL 17, 0, OSGetCurrentTeb
CALL 18, 2, OSGetExitCodeProcess
CALL 19, 2, OSGetLengthFile
CALL 20, 3, OSGetMappedFileHandle
CALL 21, 1, OSGetTickCount
CALL 22, 1, OSGetTickFrequency
CALL 23, 1, OSGetVersionNumber
//   24     OSMapViewOfObject
CALL 25, 2, OSOpenEvent
CALL 26, 2, OSOpenFile
CALL 27, 2, OSOpenMutex
CALL 28, 2, OSOutputDebugString
CALL 29, 1, OSPulseEvent
CALL 30, 2, OSQueryEvent
CALL 31, 2, OSQueryMutex
CALL 32, 4, OSReadDirectoryEntries
//   33     OSReadFile
CALL 34, 4, OSReadVirtualMemory
CALL 35, 1, OSReleaseMutex
CALL 36, 1, OSResetEvent
//   37     OSSeekFile
CALL 38, 1, OSSetCurrentPeb
CALL 39, 1, OSSetCurrentTeb
CALL 40, 1, OSSetEvent
CALL 41, 1, OSSetExitCode
CALL 42, 2, OSSetPebProcess
CALL 43, 2, OSSetSuspendedThread
CALL 44, 1, OSSleep
CALL 45, 1, OSTerminateThread
CALL 46, 1, OSTouchFile
CALL 47, 5, OSWaitForMultipleObjects
CALL 48, 3, OSWaitForSingleObject
//   49     OSWriteFile
CALL 50, 4, OSWriteVirtualMemory

#ifdef IS_64_BIT
CALL 24, 7, OSMapViewOfObject
CALL 33, 6, OSReadFile
CALL 37, 4, OSSeekFile
CALL 49, 7, OSWriteFile
#else
CALL 24, 8, OSMapViewOfObject
CALL 33, 7, OSReadFile
CALL 37, 5, OSSeekFile
CALL 49, 8, OSWriteFile
#endif
