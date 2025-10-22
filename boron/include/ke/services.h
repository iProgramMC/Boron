#pragma once

#include <status.h>
#include <handle.h>

BSTATUS OSSetPebProcess(HANDLE ProcessHandle, void* PebPtr);

BSTATUS OSSetCurrentPeb(void* Ptr);

BSTATUS OSSetCurrentTeb(void* Ptr);

void* OSGetCurrentPeb();

void* OSGetCurrentTeb();

BSTATUS OSGetTickCount(uint64_t* TickCount);

BSTATUS OSGetTickFrequency(uint64_t* TickFrequency);

BSTATUS OSGetVersionNumber(int* VersionNumber);

BSTATUS OSOutputDebugString(const char* String, size_t StringLength);
