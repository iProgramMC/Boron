/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ha/ps2.c
	
Abstract:
	This module contains the implementation of the PS/2
	device driver.
	
Author:
	iProgramInCpp - 24 September 2023
***/
#include <hal.h>
#include "ps2.h"
#include "ioapic.h"
#include "pio.h"

void HalHandleKeyboardInterrupt()
{
	LogMsg("Interrupt!  Received %d", 1);
}

void HalInitPs2()
{
	
}
