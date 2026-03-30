#ifndef BORON_HALX86_HALI_H
#define BORON_HALX86_HALI_H

#include <hal.h>

#define HAL_API // specify calling convention here if needed

void HalLockUpOtherProcessors();
void HalProcessorCrashed();

#endif//BORON_HALX86_HALI_H
