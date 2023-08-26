// Boron64 - Interrupt priority levels
#ifndef NS64_KE_IPL_H
#define NS64_KE_IPL_H

// Interrupt priority level enum
// Specification:
// Every HAL will define the following:
// IPL_NORMAL == 0
// IPL_APC
// IPL_DPC
// IPL_IPI
// IPL_CLOCK
// IPL_NOINTS.

#ifdef TARGET_AMD64
#include <hal/amd64/ipl.h>
#endif

#endif
