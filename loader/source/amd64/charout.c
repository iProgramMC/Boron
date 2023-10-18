/*************************************************************\
*                 The Boron Operating System                  *
*                       System Loader                         *
*                                                             *
*              Copyright (C) 2023 iProgramInCpp               *
*                                                             *
*                          charout.c                          *
\*************************************************************/
#include <loader.h>
#include "../charout.h"

void PortWriteByte(uint16_t portNo, uint8_t data)
{
	ASM("outb %0, %1"::"a"((uint8_t)data),"Nd"((uint16_t)portNo));
}

void PrintChar(char c)
{
	PortWriteByte(0xE9, c);
}
