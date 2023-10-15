/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ha/ioapic.h
	
Abstract:
	This header contains definitions related to the IOAPIC driver.
	
Author:
	iProgramInCpp - 15 October 2023
***/
#ifndef BORON_HAL_IAPC_IOAPIC_H
#define BORON_HAL_IAPC_IOAPIC_H

void HalIoApicSetIrqRedirect(uint8_t Vector, uint8_t Irq, uint32_t LapicId, bool Status);

#endif//BORON_HAL_IAPC_IOAPIC_H
