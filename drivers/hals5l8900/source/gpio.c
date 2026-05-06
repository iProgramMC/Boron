/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	gpio.c
	
Abstract:
	This module implements the GPIO management functions
	for the iPod GPIO driver.
	
Author:
	iProgramInCpp - 5 May 2026
***/
#include <ke.h>
#include <hal.h>
#include "gpio.h"

uintptr_t HalGpioBase, HalGpioIcBase;

#ifdef CONFIG_SMP
#error TODO: synchronize with KeSynchronizeInterrupt instead of disabling interrupts!
#endif

typedef struct
{
	uint8_t Trigger : 1;  // 1 if edge, 0 if level
	uint8_t Level : 1;
	uint8_t FlipLevel : 1;
}
GPIO_INTERRUPT, *PGPIO_INTERRUPT;

typedef struct
{
	KINTERRUPT Interrupt;
	KSPIN_LOCK InterruptSyncLock;
	GPIO_INTERRUPT Interrupts[GPIO_INTERRUPTS_PER_GROUP];
}
GPIO_INTERRUPT_GROUP, *PGPIO_INTERRUPT_GROUP;

static GPIO_INTERRUPT_GROUP HalGpioInterruptGroups[GPIO_INTERRUPT_GROUP_COUNT];

static const uint8_t HalGpioIrqNumbers[GPIO_INTERRUPT_GROUP_COUNT] = { 0x21, 0x20, 0x1f, 0x03, 0x02, 0x01, 0x00 };

void HalGpioInterruptHandler(UNUSED PKINTERRUPT Interrupt, void* Context)
{
	int GroupNumber = (int) (uintptr_t) Context;
	ASSERT(GroupNumber >= 0 && GroupNumber < GPIO_INTERRUPT_GROUP_COUNT);
	
	PGPIO_INTERRUPT_GROUP Group = &HalGpioInterruptGroups[GroupNumber];
	
	do
	{
		uint32_t Stat;
		
		Stat = GPIOIC_INT_STAT[GroupNumber];
		if (Stat == 0)
			break;
		
		for (int i = 0; i < 32; i++)
		{
			if (~Stat & (1 << i))
				continue;
			
			PGPIO_INTERRUPT GpioInt = &Group->Interrupts[i];
			
			// This bit is set.
			if (GpioInt->Trigger)
			{
				// Edge trigger
				GPIOIC_INT_STAT[GroupNumber] = 1 << i;
			}
			
			if (GpioInt->FlipLevel)
			{
				// Flip the interrupt level on the controller and in our flags
				GPIOIC_INT_LEVEL[GroupNumber] ^= 1 << i;
				GpioInt->Level ^= 1;
			}
			
			KeDispatchInterruptRequest(APPLE_IRQ_NUMBER_GPIO_TO_NORMAL(GroupNumber * GPIO_INTERRUPTS_PER_GROUP + i));
			
			GPIOIC_INT_STAT[GroupNumber] = 1 << i;
		}
	}
	while (true);
}

void HalRegisterGpioInterrupt(
	int InterruptNumber,
	bool TriggerEdge,
	bool Level,
	bool FlipLevel
)
{
	int Group = InterruptNumber >> 5;
	int Index = InterruptNumber & 0x1F;
	int Mask = ~(1 << Index);
	
	bool Restore = KeDisableInterrupts();
	
	PGPIO_INTERRUPT Interrupt = &HalGpioInterruptGroups[Group].Interrupts[Index];
	Interrupt->Trigger = TriggerEdge;
	Interrupt->Level = Level;
	Interrupt->FlipLevel = FlipLevel;
	
	// Now program the hardware.
	GPIOIC_INT_TYPE[Group] = (GPIOIC_INT_TYPE[Group] & Mask) | ((TriggerEdge ? 1 : 0) << Index);
	GPIOIC_INT_LEVEL[Group] = (GPIOIC_INT_LEVEL[Group] & Mask) | ((Level ? 1 : 0) << Index);
	
	KeRestoreInterrupts(Restore);
}

void HalEnableGpioInterrupt(int InterruptNumber)
{
	int Group = InterruptNumber >> 5;
	int Index = InterruptNumber & 0x1F;
	
	bool Restore = KeDisableInterrupts();
	
	PGPIO_INTERRUPT Interrupt = &HalGpioInterruptGroups[Group].Interrupts[Index];
	
	GPIOIC_INT_STAT[Group] = 1 << Index;
	GPIOIC_INT_ENABLE[Group] |= 1 << Index;
	
	KeRestoreInterrupts(Restore);
}

void HalDisableGpioInterrupt(int InterruptNumber)
{
	int Group = InterruptNumber >> 5;
	int Index = InterruptNumber & 0x1F;
	
	bool Restore = KeDisableInterrupts();
	
	PGPIO_INTERRUPT Interrupt = &HalGpioInterruptGroups[Group].Interrupts[Index];
	
	GPIOIC_INT_ENABLE[Group] &= ~(1 << Index);
	
	KeRestoreInterrupts(Restore);
}

void HalInitGpio()
{
	HalGpioBase = (uintptr_t) MmMapIoSpace(
		GPIO_PHYS_BASE,
		PAGE_SIZE,
		MM_PROT_READ | MM_PROT_WRITE | MM_MISC_DISABLE_CACHE,
		POOL_TAG("GPIO")
	);

	HalGpioIcBase = (uintptr_t) MmMapIoSpace(
		GPIO_IC_PHYS_BASE,
		PAGE_SIZE,
		MM_PROT_READ | MM_PROT_WRITE | MM_MISC_DISABLE_CACHE,
		POOL_TAG("GPIC")
	);

	if (!HalGpioBase || !HalGpioIcBase)
	{
		KeCrash("HAL: Error, couldn't map GPIO IC or GPIO base to memory.");
	}

	for (int i = 0; i < GPIO_INTERRUPT_GROUP_COUNT; i++)
	{
		GPIOIC_INT_STAT[i]   = GPIOIC_INT_STAT_RESET;
		GPIOIC_INT_ENABLE[i] = GPIOIC_INT_ENABLE_RESET;
	}

	// Initialize and connect each GPIO interrupt.
	for (int i = 0; i < GPIO_INTERRUPT_GROUP_COUNT; i++)
	{
		KeInitializeInterrupt(
			&HalGpioInterruptGroups[i].Interrupt,
			HalGpioInterruptHandler,
			(void*) (uintptr_t) i,
			&HalGpioInterruptGroups[i].InterruptSyncLock,
			HalGpioIrqNumbers[i],
			IPL_DEVICES0,
			false // Shared
		);
		
		KeConnectInterrupt(&HalGpioInterruptGroups[i].Interrupt);
	}

	HalSetEnabledClockGate(CLOCK_GATE_GPIO, true);
}
