/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	gpio.h
	
Abstract:
	This header defines values for the iPod GPIO driver.
	
Author:
	iProgramInCpp - 5 May 2026
***/
#pragma once

#include "pl192.h"

// Button Defines

#define BUTTON_HOLD_ID           0x1605
#define BUTTON_HOLD_IRQ          0x2D
#define BUTTON_IPT1G_HOME_ID     0x1606
#define BUTTON_IPT1G_HOME_IRQ    0x2E
#define BUTTON_IPH_HOME_ID       0x1600
#define BUTTON_IPH_HOME_IRQ      0x28
#define BUTTON_VOLUME_UP_ID      0x1601
#define BUTTON_VOLUME_UP_IRQ     0x29
#define BUTTON_VOLUME_DOWN_ID    0x1602
#define BUTTON_VOLUME_DOWN_IRQ   0x2A
#define BUTTON_RINGER_SWITCH_ID  0x1603
#define BUTTON_RINGER_SWITCH_IRQ 0x2B

#define BUTTON_IRQ_TYPE         1
#define BUTTON_IRQ_AUTO_FLIP    1
#define BUTTON_VOLUME_IRQ_LEVEL 1
#define BUTTON_OTHER_IRQ_LEVEL  0

#define REG(base, rgof) (* (volatile uint32_t*) ((uintptr_t)base + rgof))

extern uintptr_t HalGpioIcBase, HalGpioBase;

#define GPIO_PHYS_BASE    0x3E400000
#define GPIO_IC_PHYS_BASE 0x39A00000

#define GPIOIC_INT_LEVEL  (&REG(HalGpioIcBase, 0x80))
#define GPIOIC_INT_STAT   (&REG(HalGpioIcBase, 0xA0))
#define GPIOIC_INT_ENABLE (&REG(HalGpioIcBase, 0xC0))
#define GPIOIC_INT_TYPE   (&REG(HalGpioIcBase, 0xE0))

#define GPIOIC_INT_STAT_RESET         0xFFFFFFFF
#define GPIOIC_INT_ENABLE_RESET       0x00000000

#define GPIOIC_INT_TYPE_EDGE  0x0
#define GPIOIC_INT_TYPE_LEVEL 0x1
#define GPIOIC_INT_TYPE_MASK  0x1

#define GPIOIC_INT_LEVEL_LOW  0x0
#define GPIOIC_INT_LEVEL_HIGH 0x2
#define GPIOIC_INT_LEVEL_MASK 0x2

#define GPIOIC_AUTO_FLIP_DISABLED 0x0
#define GPIOIC_AUTO_FLIP_ENABLED  0x4
#define GPIOIC_AUTO_FLIP_MASK     0x4

#define CLOCK_GATE_GPIO   0x2C

#define GPIO_CON(Group)     REG(HalGpioBase, ((Group) * 0x20) + 0x00)
#define GPIO_DAT(Group)     REG(HalGpioBase, ((Group) * 0x20) + 0x04)
#define GPIO_PUD1(Group)    REG(HalGpioBase, ((Group) * 0x20) + 0x08)
#define GPIO_PUD2(Group)    REG(HalGpioBase, ((Group) * 0x20) + 0x0C)
#define GPIO_CONSLP1(Group) REG(HalGpioBase, ((Group) * 0x20) + 0x10)
#define GPIO_CONSLP2(Group) REG(HalGpioBase, ((Group) * 0x20) + 0x14)
#define GPIO_PUDSLP1(Group) REG(HalGpioBase, ((Group) * 0x20) + 0x18)
#define GPIO_PUDSLP2(Group) REG(HalGpioBase, ((Group) * 0x20) + 0x1C)

#define GPIO_FSEL   REG(HalGpioBase, 0x320)

#define GPIO_INTERRUPT_GROUP_COUNT  7

#define GPIO_INTERRUPTS_PER_GROUP   32

typedef void(*GPIO_SERVICE_ROUTINE)(void* Context1, void* Context2);

void HalRegisterGpioInterrupt(
	int InterruptNumber,
	bool TriggerEdge,
	bool Level,
	bool FlipLevel
);

void HalEnableGpioInterrupt(int InterruptNumber);

void HalDisableGpioInterrupt(int InterruptNumber);

void HalInitGpio();

bool HalGetPinStateGpio(int Port);

void HalFselGpio(int Port, int Bits);

void HalSetInputPinGpio(int Port);

void HalSetOutputPinGpio(int Port, int Bit);
