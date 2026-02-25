#include <boron.h>
#include <stdarg.h>
#include "testfmk.h"

static int CurrentTestNumber;
static bool AssertionFailuresExit = true;

static void (*Tests[])() = {
	Test1BasicOpenAndClose,
	Test2BasicRead,
	Test3,
	Test4,
	Test5,
};

// Prints both to stdout as well as to debug console.
void TestPrintf(const char * Format, ...)
{
	va_list vl;
	va_start(vl, Format);
	char Buffer[2048];
	vsnprintf(Buffer, sizeof Buffer, Format, vl);
	va_end(vl);
	
	DbgPrint("%d: %s", CurrentTestNumber, Buffer);
	OSFPrintf(FILE_STANDARD_ERROR, "%d: %s\n", CurrentTestNumber, Buffer);
}

// Reports that an assertion has failed.
bool TestAssertionFailed(
	const char* File,
	int Line,
	const char* Func,
	const char* Condition,
	const char* Format,
	...
)
{
	if (Format)
	{
		va_list vl;
		va_start(vl, Format);
		char Buffer[2048];
		vsnprintf(Buffer, sizeof Buffer, Format, vl);
		va_end(vl);
		
		DbgPrint(
			"%d: %s (%s:%d): Assertion failed.\nCondition: %s\nMessage: %s",
			CurrentTestNumber,
			Func,
			File,
			Line,
			Condition,
			Buffer
		);
		OSFPrintf(
			FILE_STANDARD_ERROR,
			"%d: %s (%s:%d): Assertion failed.\nCondition: %s\nMessage: %s\n",
			CurrentTestNumber,
			Func,
			File,
			Line,
			Condition,
			Buffer
		);
	}
	else
	{
		DbgPrint(
			"%d: %s (%s:%d): Assertion failed.\nCondition: %s\n",
			CurrentTestNumber,
			Func,
			File,
			Line,
			Condition
		);
		OSFPrintf(
			FILE_STANDARD_ERROR,
			"%d: %s (%s:%d): Assertion failed.\nCondition: %s\n\n",
			CurrentTestNumber,
			Func,
			File,
			Line,
			Condition
		);
	}
	
	if (AssertionFailuresExit)
	{
		DbgPrint("Exiting as a result of a failure.");
		OSExitProcess(1);
	}
	
	return true;
}

int _start(int ArgumentCount, char** ArgumentArray)
{
	DbgPrint("Boron Operating System Test Harness");
	DbgPrint("-----------------------------------");
	
	// Test #0: Does OSPrintf work?
	OSPrintf("Boron Operating System Test Harness\n");
	OSPrintf("-----------------------------------\n");
	
	if (ArgumentCount > 1)
	{
		// Take the first argument, and see if it's a number, or '-f'.
		const char* Argument1 = ArgumentArray[1];
		bool IsForce = strcmp(Argument1, "-f") == 0;
		
		size_t Number = 0;
		if (!IsForce)
		{
			while (*Argument1)
			{
				if (*Argument1 < '0' || *Argument1 > '9') {
					Number = 0;
					break;
				}
				
				Number = Number * 10 + *Argument1 - '0';
				Argument1++;
			}
		}
		
		bool IsNumberValid = (Number >= 1 && Number <= ARRAY_COUNT(Tests));
		
		if (!IsNumberValid && !IsForce)
		{
			const char* Program = "TestHarness";
			if (ArgumentCount > 0)
				Program = ArgumentArray[0];
			
			TestPrintf(
				"Usage:\n"
				"\t%s                    : Runs all tests, until a failure is encountered\n"
				"\t%s [test number 1-%d] : Runs a specific test number\n"
				"\t%s [-f]               : Runs all tests, assertions do not exit\n",
				Program,
				Program,
				(int) ARRAY_COUNT(Tests),
				Program
			);
			
			return 1;
		}
		
		if (IsNumberValid)
		{
			CurrentTestNumber = Number;
			Tests[Number - 1]();
			
			DbgPrint("-----------------------------------");
			DbgPrint("Test completed.");
			OSPrintf("-----------------------------------\n");
			OSPrintf("Test completed.\n");
			
			return 0;
		}
	}
	
	// Just run all of the tests.
	for (size_t i = 0; i < ARRAY_COUNT(Tests); i++)
	{
		OSPrintf("Running test %d/%d...\n", i + 1, (int) ARRAY_COUNT(Tests));
		CurrentTestNumber = i + 1;
		Tests[i]();
	}
	
	DbgPrint("-----------------------------------");
	DbgPrint("All tests completed.");
	OSPrintf("-----------------------------------\n");
	OSPrintf("All tests completed.\n");
	
	return 0;
}