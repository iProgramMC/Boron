/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ha/pic.c
	
Abstract:
	This module contains support routines for the PIC.
	
Author:
	iProgramInCpp - 16 October 2025
***/
#include <ke.h>
#include "hali.h"

uint16_t KiIplTable[] = {
	
};

void KePortWriteByteWait(uint16_t Port, uint8_t Data)
{
	KePortWriteByte(Port, Data);
	KeSpinningHint();
}

HAL_API void HalRequestIpi(UNUSED uint32_t HardwareId, UNUSED uint32_t Flags, UNUSED int Vector)
{
	DbgPrint("HalRequestIpi not implemented");
}

HAL_API void HalEndOfInterrupt(int InterruptNumber)
{
	if (InterruptNumber >= 0x28)
		KePortWriteByte(PIC2_COMMAND, PIC_CMD_EOI);
	
	KePortWriteByte(PIC1_COMMAND, PIC_CMD_EOI);
}

HAL_API void HalRegisterInterrupt(uint8_t Vector, KIPL Ipl)
{
	// TODO
	(void) Vector;
	(void) Ipl;
}

HAL_API void HalDeregisterInterrupt(uint8_t Vector, KIPL Ipl)
{
	// TODO
	(void) Vector;
	(void) Ipl;
}

HAL_API void HalInitPic()
{
	// ICW1: Initialization Command
	KePortWriteByteWait(PIC1_COMMAND, PIC_ICW1_INITIALIZE | PIC_ICW1_ENABLE_ICW4);
	KePortWriteByteWait(PIC2_COMMAND, PIC_ICW1_INITIALIZE | PIC_ICW1_ENABLE_ICW4);
	
	// ICW2: Base offset of interrupts
	KePortWriteByteWait(PIC1_DATA, PIC_INTERRUPT_BASE);
	KePortWriteByteWait(PIC2_DATA, PIC_INTERRUPT_BASE + 8);
	
	// ICW3: Primary/secondary relationship
	KePortWriteByteWait(PIC1_DATA, PIC_ICW3_PRI_CONFIG);
	KePortWriteByteWait(PIC2_DATA, PIC_ICW3_SUB_CONFIG);
	
	// ICW4: Mode
	KePortWriteByteWait(PIC1_DATA, PIC_ICW4_8086_MODE);
	KePortWriteByteWait(PIC2_DATA, PIC_ICW4_8086_MODE);
	
	// Mask ALL interrupts
	KePortWriteByteWait(PIC1_DATA, 0xFF);
	KePortWriteByteWait(PIC2_DATA, 0xFF);
}
