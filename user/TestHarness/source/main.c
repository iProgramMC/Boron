#include <boron.h>

extern void Test1();
extern void Test2();
extern void Test3();
extern void Test4();
extern void Test5();

static void (*Tests[])() = {
	Test1,
	Test2,
	Test3,
	Test4,
	Test5,
};

int _start(int ArgumentCount, char** ArgumentArray)
{
	DbgPrint("Boron Operating System Test Harness");
	DbgPrint("-----------------------------------");
	
	// Test #0: Does OSPrintf work?
	OSPrintf("Boron Operating System Test Harness\n");
	OSPrintf("-----------------------------------\n");
	
	for (size_t i = 0; i < ARRAY_COUNT(Tests); i++)
	{
		OSPrintf("Running test %d/%d...\n", i + 1, (int) ARRAY_COUNT(Tests));
		Tests[i]();
	}
	
	DbgPrint("-----------------------------------\n");
	DbgPrint("All tests have been run.\n");
	OSPrintf("-----------------------------------\n");
	OSPrintf("All tests have been run.\n");
	
	return 0;
}