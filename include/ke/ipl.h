// Boron64 - Interrupt priority levels
#ifndef NS64_KE_IPL_H
#define NS64_KE_IPL_H

// Interrupt priority level enum
typedef enum eIPL
{
	IPL_NORMAL, // business as usual
	IPL_APC,    // asynch procedure calls and page faults
	IPL_DPC,    // deferred procedure calls and the scheduler
	IPL_IPI,    // inter-processor interrupt. For TLB shootdown etc.
	IPI_CLOCK,  // for clock timers
	IPL_NOINTS, // total control of the CPU. Interrupts are disabled in this IPL and this IPL only.
}
eIPL;

#endif
