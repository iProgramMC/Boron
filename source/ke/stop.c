#include <ke.h>
#include <hal.h>

// Stops the current CPU
void KeStopCurrentCPU()
{
	KeSetInterruptsEnabled(false);
	
	while (true)
		KeWaitForNextInterrupt();
}
