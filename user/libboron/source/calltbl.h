// Num  - System Call Number
// AC   - Argument Count
// Name - System Call Export Name
//
//   Num AC Name
CALL 0,  5, OSAllocateVirtualMemory
CALL 1,  1, OSCheckIsTerminalFile
CALL 2,  1, OSCheckIsValidHandle
CALL 3,  1, OSClose
CALL 4,  0, OSCloseAllUninheritableHandles
CALL 5,  4, OSCreateEvent
CALL 6,  6, OSCreateFileCall
CALL 7,  2, OSCreateMutex
CALL 8,  4, OSCreatePipe
CALL 9,  5, OSCreateProcessInternal
CALL 10, 3, OSCreateTerminal
CALL 11, 3, OSCreateTerminalIoHandles
CALL 12, 6, OSCreateThread
CALL 13, 6, OSDeviceIoControl
CALL 14, 4, OSDuplicateHandle
CALL 15, 1, OSExitProcessInternal
CALL 16, 0, OSExitThread
CALL 17, 1, OSFlushFileCache
CALL 18, 4, OSFlushVirtualMemory
CALL 19, 2, OSForkProcessInternal
CALL 20, 4, OSFreeVirtualMemory
CALL 21, 2, OSGetAlignmentFile
CALL 22, 0, OSGetCurrentPeb
CALL 23, 0, OSGetCurrentTeb
CALL 24, 2, OSGetExitCodeProcess
CALL 25, 2, OSGetLengthFile
CALL 26, 3, OSGetMappedFileHandle
CALL 27, 1, OSGetTickCount
CALL 28, 1, OSGetTickFrequency
CALL 29, 1, OSGetVersionNumber
//   30     OSMapViewOfObject
CALL 31, 2, OSOpenEvent
CALL 32, 2, OSOpenMutex
CALL 33, 2, OSOutputDebugString
CALL 34, 1, OSPulseEvent
CALL 35, 2, OSQueryEvent
CALL 36, 2, OSQueryMutex
CALL 37, 4, OSQuerySystemInformation
CALL 38, 3, OSQueryVirtualMemoryInformation
CALL 39, 4, OSReadDirectoryEntries
//   40     OSReadFile
CALL 41, 4, OSReadVirtualMemory
CALL 42, 1, OSReleaseMutex
CALL 43, 1, OSResetEvent
//   44     OSSeekFile
CALL 45, 1, OSSetCurrentPeb
CALL 46, 1, OSSetCurrentTeb
CALL 47, 1, OSSetEvent
CALL 48, 1, OSSetExitCode
CALL 49, 3, OSSetImageNameProcess
CALL 50, 2, OSSetPebProcess
CALL 51, 2, OSSetSuspendedThread
CALL 52, 1, OSShutDownSystem
CALL 53, 1, OSSleep
CALL 54, 1, OSTerminateThread
CALL 55, 1, OSTouchFile
CALL 56, 5, OSWaitForMultipleObjects
CALL 57, 3, OSWaitForSingleObject
//   58     OSWriteFile
CALL 59, 4, OSWriteVirtualMemory

// The following system calls use at least one 64-bit parameter.
// On 32-bit, 64-bit arguments typically get passed as high/low pairs of 32-bit arguments.
#ifdef IS_64_BIT

CALL 30, 7, OSMapViewOfObject
CALL 40, 6, OSReadFile
CALL 44, 4, OSSeekFile
CALL 58, 7, OSWriteFile

#elif defined TARGET_ARM

// ARM is a special exception: 64-bit arguments placed on the stack are 8-byte aligned.
// So we need to take care of that in the following functions.

// insert a dummy padding parameter before SectionOffset low/high pair
CALL 30, 9, OSMapViewOfObject
// no change, ByteOffset occupies r2 and r3
CALL 40, 7, OSReadFile
// r0 = FileHandle, r1 = dummy padding, r2 and r3 = Offset
CALL 44, 5, OSSeekFile
// no change, ByteOffset occupies r2 and r3
CALL 58, 8, OSWriteFile

#else

CALL 30, 8, OSMapViewOfObject
CALL 40, 7, OSReadFile
CALL 44, 5, OSSeekFile
CALL 58, 8, OSWriteFile

#endif
