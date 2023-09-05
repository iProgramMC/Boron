// Boron64 - Interrupt manager
#include <hal.h>

extern InterruptServiceRoutine KeInterruptHandlers[];

void KeAssignISR(int type, InterruptServiceRoutine routine)
{
	KeInterruptHandlers[type] = routine;
}
