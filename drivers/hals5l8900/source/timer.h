#pragma once

#define CLOCK_GATE_TIMER 0x25

#define TIMER1_MEM_BASE (0x3E200000)

#define TIMER_COUNT 7
#define TIMER_IRQ   0x7

#define TIMER_STATE_START 1
#define TIMER_STATE_STOP 0
#define TIMER_STATE_MANUAL_UPDATE 2

// not sure why they're assigned so weirdly
#define TIMER_DIVIDE1  4
#define TIMER_DIVIDE2  0
#define TIMER_DIVIDE4  1
#define TIMER_DIVIDE16 2
#define TIMER_DIVIDE64 3

// We will only use a few of these timers.
//
// Timer 1 - Piezo Timer (iPod touch 1G exclusive)
// Timer 4 - Event Timer
// Timer 5 - Vibrator Timer (iPhone 3G exclusive)

// Interestingly the vibrator motor is controlled by timer 5 on the 3G, but
// by AT commands issued to the radio board on the 2G. That's strange..
#define TIMER_EVENT   4
#define TIMER_PIEZO   1
#define TIMER_VIBRATE 5

#define TIMER_0_OFFSET 0x00
#define TIMER_1_OFFSET 0x20
#define TIMER_2_OFFSET 0x40
#define TIMER_3_OFFSET 0x60
// 0x80 skipped?
#define TIMER_4_OFFSET 0xA0
#define TIMER_5_OFFSET 0xC0
#define TIMER_6_OFFSET 0xE0

#define TIMERREG(rgof)     (* (volatile uint32_t*) ((uintptr_t)HalTimerBase + rgof))
#define TIMERREGN(n, rgof) (* (volatile uint32_t*) ((uintptr_t)HalTimerBase + HalTimerOffsets[n] + rgof))

#define TIMER_CONFIG(n)        TIMERREGN(n, 0x00)
#define TIMER_STATE(n)         TIMERREGN(n, 0x04)
#define TIMER_COUNT_BUFFER(n)  TIMERREGN(n, 0x08)
#define TIMER_COUNT_BUFFER2(n) TIMERREGN(n, 0x0C)
#define TIMER_PRESCALER(n)     TIMERREGN(n, 0x10)
#define TIMER_UNKNOWN3(n)      TIMERREGN(n, 0x14)

#define TIMER_TICKSHIGH  TIMERREG(0x80)
#define TIMER_TICKSLOW   TIMERREG(0x84)
#define TIMER_UNKREG0    TIMERREG(0x88)
#define TIMER_UNKREG1    TIMERREG(0x8C)
#define TIMER_UNKREG2    TIMERREG(0x90)
#define TIMER_UNKREG3    TIMERREG(0x94)
#define TIMER_UNKREG4    TIMERREG(0x98)
#define TIMER_IRQSTAT    TIMERREG(0x10000) // Woah!!
#define TIMER_IRQLATCH   TIMERREG(0xF8)    // 0x118 on the iPodTouch2G

// TIMER_CONFIG
#define TIMER_PRODUCE_INTERRUPTS 0x7000  // INT0_EN | INT1_EN | OVF_EN
#define TIMER_DIVIDER(Divider)   ((Divider) << 8)
#define TIMER_OPTION6            (1 << 6)

#define TIMER_SPECIAL_BIT_1 0x01000000
#define TIMER_SPECIAL_BIT_2 0x02000000

#define TIMER_TICKS_PER_SEC 12000000
#define IRQS_PER_SEC 240

void HalInitTimer();

uint64_t HalGetTickCount();

uint64_t HalGetTickFrequency();

uint64_t HalGetIntTimerFrequency();

uint64_t HalGetIntTimerDeltaTicks();

uint64_t HalGetInterruptDeltaTime();

bool HalUseOneShotIntTimer();

void HalRequestInterruptInTicks(uint64_t ticks);
