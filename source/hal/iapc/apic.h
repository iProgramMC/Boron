/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ha/apic.h
	
Abstract:
	This module contains support routine prototypes for the APIC.
	
Author:
	iProgramInCpp - 16 September 2023
***/

#ifndef NS64_HAL_APIC_H
#define NS64_HAL_APIC_H

void HalSendIpi(uint32_t Processor, int Vector);
void HalBroadcastIpi(int Vector, bool IncludeSelf);
void HalApicEoi();
void HalEnableApic();
void HalCalibrateApic();
bool HalIsApicAvailable();
void HalApicSetIrqIn(uint64_t Ticks);
void HalInitIoApic();

#endif//NS64_HAL_APIC_H
