/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	i8042.c
	
Abstract:
	This module implements support for the i8042
	controller.
	
Author:
	iProgramInCpp - 2 April 2024
***/
#include "i8042.h"

void SendByte(uint16_t Port, uint8_t Value)
{
	// Stall while the output buffer is full.
	while (KePortReadByte(I8042_PORT_STATUS) & I8042_STATUS_OUTPUT_FULL)
		KeSpinningHint();
	
	KePortWriteByte(Port, Value);
}

uint8_t GetByte(uint16_t Port)
{
	// Stall while the input buffer is empty.
	while (KePortReadByte(I8042_PORT_STATUS) & I8042_STATUS_INPUT_EMPTY)
		KeSpinningHint();
	
	return KePortReadByte(Port);
}

uint8_t ReadConfig()
{
	SendByte(I8042_PORT_CMD, I8042_CMD_READ_CONFIG);
	return GetByte(I8042_PORT_DATA);
}

void WriteConfig(uint8_t Config)
{
	SendByte(I8042_PORT_CMD,  I8042_CMD_WRITE_CONFIG);
	SendByte(I8042_PORT_DATA, Config);
}
