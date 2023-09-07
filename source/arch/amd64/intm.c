// Boron64 - Interrupt manager
#include <arch.h>

extern InterruptServiceRoutine KeInterruptHandlers[];

void KeAssignISR(int type, InterruptServiceRoutine routine)
{
	KeInterruptHandlers[type] = routine;
}
