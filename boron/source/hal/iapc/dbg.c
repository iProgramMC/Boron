/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ha/dbg.c
	
Abstract:
	This module implements the debug terminal functions for
	the AMD64 platform.
	
Author:
	iProgramInCpp - 15 October 2023
***/
#include <hal.h>
#include "../../arch/amd64/pio.h"
#include "uart.h"

#define USE_SERIAL_COM1

#ifdef USE_SERIAL_COM1

extern bool HalpIsSerialAvailable;

static short HalpGetUartPort()
{
	return 0x3F8;
}

static void HalpPrintCharacterSerial(char c);

void HalDebugTerminalInit()
{
	HalpIsSerialAvailable = false;
	
	short Port = HalpGetUartPort();
	
	// Disable All Interrupts
	KePortWriteByte (Port+1, IER_NOINTS);
	
	// Enable DLAB (Set baud rate divisor)
	KePortWriteByte (Port+3, DLAB_ENABLE);
	
	// Set Divisor to 3 -- 38400 baud
	KePortWriteByte (Port+0, BAUD_DIVISOR & 0xFF); 
	KePortWriteByte (Port+1, BAUD_DIVISOR  >>  8);
	
	// Set the data parity and bit size. 
	KePortWriteByte (Port+3, LCR_8BIT_DATA | LCR_PAR_NONE);
	
	// Enable FIFO
	KePortWriteByte (Port+2, FCR_RXTRIG | FCR_XFRES | FCR_RFRES | FCR_ENABLE);
	
	// Prepare modem control register
	KePortWriteByte (Port+4, MCR_OUT1 | MCR_RTS | MCR_DTR); // IRQs enabled, RTS/DSR set
	
	// set in loopback mode to test the serial chip
	KePortWriteByte (Port+4, MCR_LOOPBK | MCR_OUT1 | MCR_OUT2 | MCR_RTS);
	
	// Send a check byte, and check if we get it back
	KePortWriteByte (Port+0, S_CHECK_BYTE);
	
	if (KePortReadByte (Port + 0) != S_CHECK_BYTE)
	{
		// Nope. Return
		//return;
	}
	
	// Set this serial port to normal operation
	KePortWriteByte (Port+4, MCR_OUT1 | MCR_OUT2 | MCR_RTS | MCR_DTR); // IRQs enabled, OUT#1 and OUT#2 bits, no loop-back
	
	// Enable interrupts again
	KePortWriteByte (Port+1, IER_ERBFI | IER_ETBEI);
	
	HalpIsSerialAvailable = true;
	
	HalpPrintCharacterSerial('\n');
	HalpPrintCharacterSerial('\n');
	HalpPrintCharacterSerial('\n');
	HalpPrintCharacterSerial('\r');
}

static bool HalpUartIsTransmitEmpty()
{
	short Port = HalpGetUartPort();
	return((KePortReadByte(Port + 5) & 0x20) != 0);
}

static void HalpPrintCharacterSerial(char c)
{
	if (!HalpIsSerialAvailable)
		return;
	
	while (!HalpUartIsTransmitEmpty())
		KeSpinningHint();
	
	KePortWriteByte(HalpGetUartPort(), c);
}

void HalPrintStringDebug(const char* str)
{
	if (!HalpIsSerialAvailable)
		return;
	
	while (*str)
	{
		if (*str == '\n')
			HalpPrintCharacterSerial('\r');
		
		HalpPrintCharacterSerial(*str);
		
		str++;
	}
}


#else

void HalDebugTerminalInit()
{
	// No init required for the 0xE9 hack
}

void HalPrintStringDebug(const char* str)
{
	while (*str)
	{
		KePortWriteByte(0xE9, *str);
		str++;
	}
}

#endif//USE_SERIAL_COM1
