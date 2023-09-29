/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	hal/iapc/pit.c
	
Abstract:
	This module implements the interface for the PIT device.
	Used to calibrate the LAPIC timer.
	
Author:
	iProgramInCpp - 29 September 2023
***/
#include <hal.h>
#include "pit.h"
#include "../../arch/amd64/pio.h"

#define PIT_DATA_PORT (0x40) // channel 0
#define PIT_COMD_PORT (0x43) // channel 0

#define PIT_MODE_RATE_GENERATOR (0x04)
#define PIT_MODE_ACCESS_LOHI (0x30)

uint16_t HalReadPit()
{
	uint16_t Data;
	
	// Access the PIT the following way:
	// * Channel 0
	// * Latch count value command
	KePortWriteByte(PIT_COMD_PORT, 0x00);
	
	Data  = KePortReadByte(PIT_DATA_PORT);
	Data |= KePortReadByte(PIT_DATA_PORT) << 8;
	
	return Data;
}

void HalWritePit(uint16_t Data)
{
	// Access the PIT the following way:
	// * Bits 6 and 7 - Channel (0)
	// * Bits 4 and 5 - Access type (low byte and high byte)
	// * Bits 1 to 3  - Mode of operation (rate generator)
	// * Bit 0        - Write mode (0 - 16-bit binary)
	KePortWriteByte(PIT_COMD_PORT, PIT_MODE_ACCESS_LOHI | PIT_MODE_RATE_GENERATOR);
	
	KePortWriteByte(PIT_DATA_PORT, Data & 0xFF);
	KePortWriteByte(PIT_DATA_PORT, Data >> 8);
}
