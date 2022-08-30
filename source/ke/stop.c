#include <ke.h>
#include <hal.h>

// Stops the current CPU
void KeStopCurrentCPU()
{
	HalSetInterruptsEnabled(false);
	
	while (true)
		HalWaitForNextInterrupt();
}
