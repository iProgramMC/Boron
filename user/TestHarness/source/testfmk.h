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

//#define DEBUG_ASSERTIONS

#ifdef DEBUG_ASSERTIONS

#define TestAssertMsg(condition, ...) do { \
	if ((condition)) { \
		TestPrintf("%s (%s:%d): assertion '%s' succeeded.", __func__, __FILE__, __LINE__, #condition); \
	} else { \
		TestAssertionFailed(__FILE__, __LINE__, __func__, #condition, __VA_ARGS__); \
	} \
} while (0)

#define TestAssert(condition) do { \
	if ((condition)) { \
		TestPrintf("%s (%s:%d): assertion '%s' succeeded.", __func__, __FILE__, __LINE__, #condition); \
	} else { \
		TestAssertionFailed(__FILE__, __LINE__, __func__, #condition, NULL); \
	} \
} while (0)

#else

#define TestAssertMsg(condition, ...) \
	((void)((condition) || TestAssertionFailed(__FILE__, __LINE__, __func__, #condition, __VA_ARGS__)))

#define TestAssert(condition) \
	((void)((condition) || TestAssertionFailed(__FILE__, __LINE__, __func__, #condition, NULL)))

#endif
