/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	arch/amd64/pio.c
	
Abstract:
	This module implements x86 specific Port I/O functions.
	
Author:
	iProgramInCpp - 7 September 2023
***/
#include <main.h>
#include "pio.h"

uint8_t KePortReadByte(uint16_t portNo)
{
    uint8_t rv;
    ASM("inb %1, %0" : "=a" (rv) : "dN" (portNo));
    return rv;
}

void KePortWriteByte(uint16_t portNo, uint8_t data)
{
	ASM("outb %0, %1"::"a"((uint8_t)data),"Nd"((uint16_t)portNo));
}
