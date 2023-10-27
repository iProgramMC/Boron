#include <ke.h>
#include <hal.h>

void KiSetupIdt();

void KeInitArchUP()
{
	KiSetupIdt();
}

void KeInitArchMP()
{
	KeInitCPU();
	KeLowerIPL(IPL_NORMAL);
	HalInitSystemMP();
}
