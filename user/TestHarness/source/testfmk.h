#pragma once

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

extern void Test1BasicOpenAndClose();
extern void Test2BasicRead();
extern void Test3();
extern void Test4();
extern void Test5();
