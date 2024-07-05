/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	arch/amd64/pio.h
	
Abstract:
	This module implements x86 specific Port I/O functions.
	
Author:
	iProgramInCpp - 7 September 2023
***/

#ifndef NS64_PIO_H
#define NS64_PIO_H

#include <stdint.h>

uint8_t KePortReadByte(uint16_t portNo);
void KePortWriteByte(uint16_t portNo, uint8_t data);
uint16_t KePortReadWord(uint16_t portNo);
void KePortWriteWord(uint16_t portNo, uint16_t data);
uint32_t KePortReadDword(uint16_t portNo);
void KePortWriteDword(uint16_t portNo, uint32_t data);

#endif//NS64_PIO_H
