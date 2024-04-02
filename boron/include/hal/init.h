/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	hal/init.h
	
Abstract:
	This header file contains the definition for the 
	HAL function pointer structure and miscellaneous
	HAL initialization details.
	
Author:
	iProgramInCpp - 27 October 2023
***/
#ifndef BORON_HAL_INIT_H
#define BORON_HAL_INIT_H

#include <main.h>

#define HAL_VFTABLE_LOADED (1 << 0)

// Function pointer definitions
typedef void(*PFHAL_END_OF_INTERRUPT)(void);
typedef void(*PFHAL_REQUEST_INTERRUPT_IN_TICKS)(uint64_t Ticks);
typedef void(*PFHAL_REQUEST_IPI)(uint32_t LapicId, uint32_t Flags, int Vector);
typedef void(*PFHAL_INIT_SYSTEM_UP)(void);
typedef void(*PFHAL_INIT_SYSTEM_MP)(void);
typedef void(*PFHAL_DISPLAY_STRING)(const char* Message);
typedef void(*PFHAL_CRASH_SYSTEM)(const char* Message) NO_RETURN;
typedef bool(*PFHAL_USE_ONE_SHOT_INT_TIMER)(void);
typedef void(*PFHAL_PROCESSOR_CRASHED)(void);
typedef uint64_t(*PFHAL_GET_INT_TIMER_FREQUENCY)(void);
typedef uint64_t(*PFHAL_GET_TICK_COUNT)(void);
typedef uint64_t(*PFHAL_GET_TICK_FREQUENCY)(void);
typedef uint64_t(*PFHAL_GET_INT_TIMER_DELTA_TICKS)(void);
typedef void(*PFHAL_IOAPIC_SET_IRQ_REDIRECT)(uint8_t Vector, uint8_t Irq, uint32_t LapicId, bool Status);

typedef struct
{
	uint64_t Flags;
	PFHAL_END_OF_INTERRUPT EndOfInterrupt;
	PFHAL_REQUEST_INTERRUPT_IN_TICKS RequestInterruptInTicks;
	PFHAL_REQUEST_IPI RequestIpi;
	PFHAL_INIT_SYSTEM_UP InitSystemUP;
	PFHAL_INIT_SYSTEM_MP InitSystemMP;
	PFHAL_DISPLAY_STRING DisplayString;
	PFHAL_CRASH_SYSTEM CrashSystem;
	PFHAL_USE_ONE_SHOT_INT_TIMER UseOneShotIntTimer;
	PFHAL_PROCESSOR_CRASHED ProcessorCrashed;
	PFHAL_GET_INT_TIMER_FREQUENCY GetIntTimerFrequency;
	PFHAL_GET_TICK_COUNT GetTickCount;
	PFHAL_GET_TICK_FREQUENCY GetTickFrequency;
	PFHAL_GET_INT_TIMER_DELTA_TICKS GetIntTimerDeltaTicks;
	PFHAL_IOAPIC_SET_IRQ_REDIRECT IoApicSetIrqRedirect;
}
HAL_VFTABLE, *PHAL_VFTABLE;

void HalInitializeTemporary();

#endif//BORON_HAL_INIT_H
