#pragma once

#define CRASH(...) do {    \
	DbgPrint("CRASH:");    \
	DbgPrint(__VA_ARGS__); \
	OSExitThread();        \
} while (0)
