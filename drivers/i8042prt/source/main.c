/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	main.c
	
Abstract:
	This module implements the main function for the
	i8042 device driver.
	
Author:
	iProgramInCpp - 2 April 2024
***/
#include <ke.h>
#include <io.h>
#include <hal.h>

#include "i8042.h"
#include "kbd.h"

PDRIVER_OBJECT I8042DriverObject;

int AllocateVector(PKIPL Ipl, KIPL Default)
{
	int Vector = -1;
	
	*Ipl = Default;
	while ((Vector = KeAllocateInterruptVector(*Ipl)) < 0 && *Ipl < IPL_CLOCK)
		(*Ipl)++;
	
	return Vector;
}

BSTATUS InitializeDevice()
{
	// Disable primary and secondary PS/2 ports.
	SendByte(I8042_PORT_CMD, I8042_CMD_DISABLE_PORT_1);
	SendByte(I8042_PORT_CMD, I8042_CMD_DISABLE_PORT_2);
	
	// Reconfigure the controller.
	uint8_t Config = ReadConfig();
	
	// HACK: At least one of my test rigs returns a bit flipped configuration
	// byte. Not sure why. If two specific bits that should be zero aren't,
	// perform a bit-flip on the entire config.
	if ((Config & I8042_CONFIG_ZERO_BIT_3) &&
	    (Config & I8042_CONFIG_ZERO_BIT_7))
		Config = ~Config;
	
	// Enable the keyboard port and translation
	Config |= I8042_CONFIG_INT_PORT_1;
	Config |= I8042_CONFIG_TRANSLATION;
	
	// If port 2 was affected by our command, enable the interrupt for it.
	if (Config & I8042_CONFIG_DISABLE_2)
		Config |= I8042_CONFIG_INT_PORT_2;
	
	WriteConfig(Config);
	
	// Enable the ports.
	SendByte(I8042_PORT_CMD, I8042_CMD_ENABLE_PORT_1);
	if (Config & I8042_CONFIG_DISABLE_2)
		//SendByte(I8042_PORT_CMD, I8042_CMD_ENABLE_PORT_2);
		(void) 0;
	
	// Allocate an interrupt for the keyboard.
#ifdef TARGET_AMD64

	KIPL IplKbd, IplMou;
	int VectorKbd = AllocateVector(&IplKbd, IPL_DEVICES0);
	int VectorMou = AllocateVector(&IplMou, IPL_DEVICES0);
	
	uint32_t LapicId = KeGetCurrentPRCB()->LapicId;
	
	bool Restore = KeDisableInterrupts();
	
	// Ok, now set the IRQ redirects.
	HalIoApicSetIrqRedirect(VectorKbd, I8042_IRQ_KBD, LapicId, true);
	HalIoApicSetIrqRedirect(VectorMou, I8042_IRQ_MOU, LapicId, true);

#elif defined TARGET_I386

	const KIPL IplKbd = IPL_DEVICES0;
	const int VectorKbd = PIC_INTERRUPT_BASE;
	
	bool Restore = KeDisableInterrupts();

#else
#error If you're using the i8042prt driver for another architecture, please define the interrupt method for it.
#endif
	
	// Initialize the keyboard.
	KbdInitialize(VectorKbd, IplKbd);
	
	KeRestoreInterrupts(Restore);
	return STATUS_SUCCESS;
}

BSTATUS KbdUnload(UNUSED PDRIVER_OBJECT DriverObject)
{
	LogMsg("I8042prt: Tried to unload.  That's not supported.");
	return STATUS_UNIMPLEMENTED;
}

BSTATUS DriverEntry(PDRIVER_OBJECT DriverObject)
{
	I8042DriverObject = DriverObject;
	
	BSTATUS Status;
	
	// Create the device object for each device.
	Status = KbdCreateDeviceObject();
	if (FAILED(Status))
	{
		LogMsg("I8042prt: Cannot create keyboard device object.");
		return Status;
	}
	
	Status = InitializeDevice();
	if (FAILED(Status))
	{
		LogMsg("I8042prt: Cannot initialize keyboard driver.");
		return Status;
	}
	
	return STATUS_SUCCESS;
}
