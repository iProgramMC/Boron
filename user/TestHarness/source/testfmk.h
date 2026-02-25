#pragma once

// Shorthand for RtlGetStatusString
#define ST(st) RtlGetStatusString(st)

void TestPrintf(const char *, ...);

void TestAssertionFailed(
	const char* File,
	const char* Line, 
	const char* Func,
	const char* Condition,
	const char* Format,
	...
);

#define TestAssert(condition, ...) \
	((condition) || TestAssertionFailed(__FILE__, __LINE__, __func__, #condition, __VA_ARGS__))

extern void Test1BasicOpenAndClose();
extern void Test2BasicRead();
extern void Test3();
extern void Test4();
extern void Test5();
