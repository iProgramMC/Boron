// Boron64 - Interrupt priority levels
#include <ke.h>

eIPL KeGetIPL()
{
	CPU* thisCPU = KeGetCPU();
	return thisCPU->Ipl;
}

eIPL KeRaiseIPL(eIPL newIPL)
{
	CPU* thisCPU = KeGetCPU();
	eIPL oldIPL = thisCPU->Ipl;
	
	if (oldIPL == newIPL)
		return oldIPL; // no changes
	
	if (oldIPL > newIPL)
	{
		LogMsg("Error, can't raise the IPL to a lower level than we are (old %d, new %d)", oldIPL, newIPL);
		// TODO: panic here
		return oldIPL;
	}
	
	KeOnUpdateIPL(newIPL, oldIPL);
	
	// Set the current IPL
	thisCPU->Ipl = newIPL;
	return oldIPL;
}

// similar logic, except we will also call DPCs if needed
eIPL KeLowerIPL(eIPL newIPL)
{
	CPU* thisCPU = KeGetCPU();
	eIPL oldIPL = thisCPU->Ipl;
	
	if (oldIPL == newIPL)
		return oldIPL; // no changes
	
	if (oldIPL < newIPL)
	{
		LogMsg("Error, can't lower the IPL to a higher level than we are (old %d, new %d)", oldIPL, newIPL);
		// TODO: panic here
		return oldIPL;
	}
	
	// Set the current IPL
	thisCPU->Ipl = newIPL;
	
	KeOnUpdateIPL(newIPL, oldIPL);
	
	// TODO (updated): Issue a self-IPI to call DPCs.
	
	// TODO: call DPCs
	// ideally we'd have something as follows:
	// while (thisCPU->Ipl > newIPL) {
	//     thisCPU->Ipl--;  // lower the IPL by one stage..
	//     executeDPCs();     // execute the DPCs at this IPL
	// }                      // continue until we are at the current IPL
	// executeDPCs();         // do that one more time because we weren't in the while loop
	
	return oldIPL;
}
