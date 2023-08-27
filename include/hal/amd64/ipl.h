// Boron64 - AMD64 - Interrupt priority levels
#ifndef NS64_HAL_AMD64_IPL_H
#define NS64_HAL_AMD64_IPL_H

typedef enum eIPL
{
	IPL_NORMAL = 0x0, // business as usual
	IPL_APC    = 0x3, // asynch procedure calls. Page faults only allowed up to this IPL
	IPL_DPC    = 0x4, // deferred procedure calls and the scheduler
	IPI_CLOCK  = 0xE, // for clock timers
	IPL_NOINTS = 0xF, // total control of the CPU. Interrupts are disabled in this IPL and this IPL only.
}
eIPL;

#endif//NS64_HAL_AMD64_IPL_H