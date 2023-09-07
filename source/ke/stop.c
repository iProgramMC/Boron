#include <ke.h>
#include <arch.h>

// Stops the current CPU
void KeStopCurrentCPU()
{
	KeSetInterruptsEnabled(false);
	
	while (true)
		KeWaitForNextInterrupt();
}
