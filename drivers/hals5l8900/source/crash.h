#pragma once

#include <main.h>
#include "hali.h"

HAL_API
void HalProcessorCrashed();

NO_RETURN
void HalCrashSystem(const char* Message);
