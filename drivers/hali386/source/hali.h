#pragma once

#include <hal.h>

#define HAL_API // specify calling convention here if needed

#define PIT_TICK_FREQUENCY   (1193182)
#define PIT_CHANNEL_0_PORT   (0x40)

#define PIC_INTERRUPT_BASE   (0x20)

#define PIC1_COMMAND         (0x20)
#define PIC1_DATA            (0x21)
#define PIC2_COMMAND         (0xA0)
#define PIC2_DATA            (0xA1)

#define PIC_CMD_EOI          (0x20)

// From the 8259A datasheet.
#define PIC_ICW1_ENABLE_ICW4 (1 << 0)
#define PIC_ICW1_INITIALIZE  (1 << 4) // always 1 in the data sheet
#define PIC_ICW3_PRI_CONFIG  (1 << 2) // there is a sub PIC and it's IRQ 2
#define PIC_ICW3_SUB_CONFIG  (2)      // the sub PIC cascades at IRQ 2
#define PIC_ICW4_8086_MODE   (1 << 0)

// ====== Timer ======
void HalUpdatePitClock();

HAL_API uint64_t HalGetTickFrequency();

HAL_API uint64_t HalGetTickCount();

HAL_API bool HalUseOneShotIntTimer();

HAL_API uint64_t HalGetIntTimerFrequency();

HAL_API void HalRequestInterruptInTicks(uint64_t ticks);

HAL_API uint64_t HalGetIntTimerDeltaTicks();

// ====== Interrupt Controller ======
HAL_API void HalInitPic();

HAL_API void HalEndOfInterrupt(int InterruptNumber);



