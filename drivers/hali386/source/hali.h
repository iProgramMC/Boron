#pragma once

#include <hal.h>

#define HAL_API // specify calling convention here if needed

#define PIT_TICK_FREQUENCY  (1193182)
#define PIT_CHANNEL_0_PORT  (0x40)

#define PIC_MAIN_COMMAND    (0x20)
#define PIC_MAIN_DATA       (0x21)
#define PIC_SUB_COMMAND     (0xA0)
#define PIC_SUB_DATA        (0xA1)

#define PIC_CMD_EOI  (0x20)

// ====== Timer ======
void HalUpdatePitClock();

HAL_API uint64_t HalGetTickFrequency();

HAL_API uint64_t HalGetTickCount();

HAL_API bool HalUseOneShotIntTimer();

HAL_API uint64_t HalGetIntTimerFrequency();

HAL_API void HalRequestInterruptInTicks(uint64_t ticks);

HAL_API uint64_t HalGetIntTimerDeltaTicks();



