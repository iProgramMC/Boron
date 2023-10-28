#include <ke.h>
#include <hal.h>
#include "archi.h"

int KiVectorCrash,
    KiVectorTlbShootdown,
    KiVectorDpcIpi;

void KiSetupIdt();

void KeInitArchUP()
{
	KiSetupIdt();
	
	// Initialize interrupt vectors for certain things
	KiVectorCrash        = KeAllocateInterruptVector(IPL_NOINTS);
	KiVectorTlbShootdown = KeAllocateInterruptVector(IPL_NOINTS);
	KiVectorDpcIpi       = KeAllocateInterruptVector(IPL_DPC);
	
	KeRegisterInterrupt(KiVectorDpcIpi,       KiHandleDpcIpi);
	KeRegisterInterrupt(KiVectorTlbShootdown, KiHandleTlbShootdownIpi);
	KeRegisterInterrupt(KiVectorCrash,        KiHandleCrashIpi);
}

void KeInitArchMP()
{
	KeInitCPU();
	KeLowerIPL(IPL_NORMAL);
	HalInitSystemMP();
}
