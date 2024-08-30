/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/amd64/pio.c
	
Abstract:
	This module implements x86 specific Port I/O functions.
	
Author:
	iProgramInCpp - 7 September 2023
***/
#include <arch.h>

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

uint16_t KePortReadWord(uint16_t portNo)
{
    uint16_t rv;
    ASM("inw %1, %0" : "=a" (rv) : "dN" (portNo));
    return rv;
}

void KePortWriteWord(uint16_t portNo, uint16_t data)
{
	ASM("outw %0, %1"::"a"((uint16_t)data),"Nd"((uint16_t)portNo));
}


uint32_t KePortReadDword(uint16_t portNo)
{
    uint32_t rv;
    ASM("inl %1, %0" : "=a" (rv) : "dN" (portNo));
    return rv;
}

void KePortWriteDword(uint16_t portNo, uint32_t data)
{
	ASM("outl %0, %1"::"a"((uint32_t)data),"Nd"((uint16_t)portNo));
}
