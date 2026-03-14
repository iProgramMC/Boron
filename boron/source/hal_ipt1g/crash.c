#include "hali.h"

NO_RETURN
void HalIpodProcessorCrashed()
{
	DbgPrint("%s NYI", __func__);
	
	KeDisableInterrupts();
	while (1) {
		ASM("wfi");
	}
}

NO_RETURN
void HalIpodCrashSystem(const char* Message)
{
	DbgPrint("%s(%s) NYI", __func__, Message);
	
	KeDisableInterrupts();
	while (1) {
		ASM("wfi");
	}
}

void HalIpodDisplayString(const char* Message)
{
	DbgPrint("%s(%s) NYI", __func__, Message);
}
