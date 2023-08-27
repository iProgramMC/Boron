// Boron64 - Interrupt manager
#include <hal.h>

extern InterruptServiceRoutine HalInterruptHandlers[];

void HalAssignISR(int type, InterruptServiceRoutine routine)
{
	HalInterruptHandlers[type] = routine;
}
