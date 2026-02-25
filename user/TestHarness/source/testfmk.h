#pragma once
#include <boron.h>

// Shorthand for RtlGetStatusString
#define ST(st) RtlGetStatusString(st)

void TestPrintf(const char *, ...);

bool TestAssertionFailed(
	const char* File,
	int Line,
	const char* Func,
	const char* Condition,
	const char* Format,
	...
);

#define TestAssertMsg(condition, ...) \
	((void)((condition) || TestAssertionFailed(__FILE__, __LINE__, __func__, #condition, __VA_ARGS__)))

#define TestAssert(condition) \
	((void)((condition) || TestAssertionFailed(__FILE__, __LINE__, __func__, #condition, NULL)))
