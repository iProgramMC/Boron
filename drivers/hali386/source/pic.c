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
	0xFFFF, // IPL_NORMAL
	0xFFFF, // IPL_1
	0xFFFF, // IPL_2
	0xFFFF, // IPL_APC
	0xFFFF, // IPL_DPC
	0xFFFF, // IPL_DEVICES0
	0xFFFF, // IPL_DEVICES1
	0xFFFF, // IPL_DEVICES2
	0xFFFF, // IPL_DEVICES3
	0xFFFF, // IPL_DEVICES4
	0xFFFF, // IPL_DEVICES5
	0xFFFF, // IPL_DEVICES6
	0xFFFF, // IPL_DEVICES7
	0xFFFF, // IPL_DEVICES8
	0xFFFF, // IPL_CLOCK
	0xFFFF, // IPL_NOINTS
};
static_assert(ARRAY_COUNT(KiIplTable) == IPL_COUNT);

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

HAL_API void HalPicRegisterInterrupt(uint8_t Vector, KIPL Ipl)
{
	DbgPrint("HalPicRegisterInterrupt: %d, %d", Vector, Ipl);
	if (Vector < PIC_INTERRUPT_BASE || Vector >= PIC_INTERRUPT_BASE + 16)
	{
		DbgPrint("HalRegisterInterrupt: Dropping vector %zu", Vector);
		return;
	}
	
	bool Restore = KeDisableInterrupts();
	
	Vector -= PIC_INTERRUPT_BASE;
	for (KIPL i = 0; i < Ipl; i++)
		KiIplTable[i] &= ~(1 << Vector);
	
	KeRestoreInterrupts(Restore);
	
	// TODO: just unmask everything for now.  Should we even use the other masks?
	KePortWriteByteWait(PIC1_DATA, KiIplTable[0] & 0xFF);
	KePortWriteByteWait(PIC2_DATA, KiIplTable[0] >> 8);
}

// NOTE: There are no more uses of this interrupt once this function is called. I hope.
HAL_API void HalPicDeregisterInterrupt(uint8_t Vector, KIPL Ipl)
{
	if (Vector < PIC_INTERRUPT_BASE || Vector >= PIC_INTERRUPT_BASE + 16)
	{
		DbgPrint("HalRegisterInterrupt: Dropping vector %zu", Vector);
		return;
	}
	
	bool Restore = KeDisableInterrupts();
	
	Vector -= PIC_INTERRUPT_BASE;
	for (KIPL i = 0; i < Ipl; i++)
		KiIplTable[i] |= 1 << Vector;
	
	KeRestoreInterrupts(Restore);
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
