#include "hali.h"

NO_RETURN
void HalProcessorCrashed()
{
	DbgPrint("%s NYI", __func__);
	
	KeDisableInterrupts();
	while (1) {
		ASM("wfi");
	}
}

NO_RETURN
void HalCrashSystem(const char* Message)
{
	DbgPrint("%s(%s) NYI", __func__, Message);
	
	KeDisableInterrupts();
	while (1) {
		ASM("wfi");
	}
}

void HalDisplayString(const char* Message)
{
	DbgPrint("%s(%s) NYI", __func__, Message);
}
