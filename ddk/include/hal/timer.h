/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	hal/timer.h
	
Abstract:
	This header file contains the definitions for the API
	for the generic system timer (GST) implemented by the
	HAL.
	
Author:
	iProgramInCpp - 29 September 2023
***/
#ifndef BORON_HAL_TIMER_H
#define BORON_HAL_TIMER_H

//
// The timer interface is split into two parts:
// * Monotonic timer (MT)
// * One shot timer (OST)
// * Periodic Timer (PT)
//
// With the MT, you get a monotonically increasing tick count whose frequency
// never changes. However, you can't set up any kind of interrupt.
//
// With the OST, you get a timer that, when a set number of ticks passes, fires
// an interrupt. However, you don't get a monotonically increasing tick count.
//
// The OST is mutually exclusive to the PT (there can't be both at the same time),
// and HalUseOneShotTimer() determines whether to program for the OST or the PT.
//
// Both the OST and PT are called "interrupt timers" (IT). Use the IT term when
// referring to both at the same time, such as in HalGetItTicksPerSecond().
//

// ==== Monotonic timer ====
// Gets the frequency of the monotonic timer.
uint64_t HalGetTicksPerSecond();

// Gets the 
uint64_t HalGetTickCount();

// ==== Interrupt timer (OST + PT common) ====
// Get the frequency of the interrupt timer.
uint64_t HalGetItTicksPerSecond();

// Whether to use the OST (true) or the PT (false).
bool HalUseOneShotTimer();

// ==== One Shot Timer ====
// Requests an interrupt in a certain amount of ticks.
// This is a feature of the OST only, don't call this if HalUseOneShotTimer() is false.
void HalRequestInterruptInTicks(uint64_t ticks);

// ==== Periodic Timer ====
// Get the time (in ticks) between interrupts.
// If 1, then every tick represents an interrupt, and HalGetItTicksPerSecond() essentially
// represents the amount of interrupts in a second.
uint64_t HalGetInterruptDeltaTime();

#endif//BORON_HAL_TIMER_H
