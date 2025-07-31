#pragma once

#define CRASH(...) do {    \
	DbgPrint("CRASH:");    \
	DbgPrint(__VA_ARGS__); \
	OSExitThread();        \
} while (0)

#define CHECK_STATUS(nr) do { \
	if (FAILED(Status))       \
		CRASH("Test %d: Failure at line %d.  %d (%s)", nr, __LINE__, Status, RtlGetStatusString(Status)); \
} while (0)
