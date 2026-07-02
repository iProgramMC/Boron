/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	main.c
	
Abstract:
	This module implements the main function for the
	iPod/iPhone button device driver.
	
Author:
	iProgramInCpp - 6 May 2026
***/
#include <hal.h>
#include <io.h>
#include <ldr.h>
#include <string.h>

// Button Defines

#define BUTTON_HOLD_ID           0x1605
#define BUTTON_HOLD_IRQ          0x2D
#define BUTTON_IPT_HOME_ID       0x1606
#define BUTTON_IPT_HOME_IRQ      0x2E
#define BUTTON_IPH_HOME_ID       0x1600
#define BUTTON_IPH_HOME_IRQ      0x28
#define BUTTON_VOLUME_UP_ID      0x1601
#define BUTTON_VOLUME_UP_IRQ     0x29
#define BUTTON_VOLUME_DOWN_ID    0x1602
#define BUTTON_VOLUME_DOWN_IRQ   0x2A
#define BUTTON_RINGER_SWITCH_ID  0x1603
#define BUTTON_RINGER_SWITCH_IRQ 0x2B

#define BUTTON_IPT2G_HOME_ID         0xC01
#define BUTTON_IPT2G_HOME_IRQ        0x79
#define BUTTON_IPT2G_HOLD_ID         0xC02
#define BUTTON_IPT2G_HOLD_IRQ        0x7A
#define BUTTON_IPT2G_VOLUME_UP_ID    0x902
#define BUTTON_IPT2G_VOLUME_UP_IRQ   0x62
#define BUTTON_IPT2G_VOLUME_DOWN_ID  0xC00
#define BUTTON_IPT2G_VOLUME_DOWN_IRQ 0x78

#define BUTTON_IRQ_TYPE         1
#define BUTTON_IRQ_AUTO_FLIP    1
#define BUTTON_VOLUME_IRQ_LEVEL 1
#define BUTTON_OTHER_IRQ_LEVEL  0
#define BUTTON_IPT2G_IRQ_LEVEL  1

PDRIVER_OBJECT gDriverObject;

typedef struct
{
	uint16_t Id;
	uint8_t Irq;
	uint8_t Type : 1;
	uint8_t Level : 1;
	uint8_t AutoFlip : 1;
}
BUTTON_INTERRUPT_DATA, *PBUTTON_INTERRUPT_DATA;

static const BUTTON_INTERRUPT_DATA gButtonInterruptData[] =
{
	{ BUTTON_HOLD_ID,          BUTTON_HOLD_IRQ,          BUTTON_IRQ_TYPE, BUTTON_OTHER_IRQ_LEVEL,  BUTTON_IRQ_AUTO_FLIP },
	{ BUTTON_IPT_HOME_ID,      BUTTON_IPT_HOME_IRQ,      BUTTON_IRQ_TYPE, BUTTON_OTHER_IRQ_LEVEL,  BUTTON_IRQ_AUTO_FLIP },
	{ BUTTON_IPH_HOME_ID,      BUTTON_IPH_HOME_IRQ,      BUTTON_IRQ_TYPE, BUTTON_OTHER_IRQ_LEVEL,  BUTTON_IRQ_AUTO_FLIP },
	{ BUTTON_RINGER_SWITCH_ID, BUTTON_RINGER_SWITCH_IRQ, BUTTON_IRQ_TYPE, BUTTON_OTHER_IRQ_LEVEL,  BUTTON_IRQ_AUTO_FLIP },
	{ BUTTON_VOLUME_UP_ID,     BUTTON_VOLUME_UP_IRQ,     BUTTON_IRQ_TYPE, BUTTON_VOLUME_IRQ_LEVEL, BUTTON_IRQ_AUTO_FLIP },
	{ BUTTON_VOLUME_DOWN_ID,   BUTTON_VOLUME_DOWN_IRQ,   BUTTON_IRQ_TYPE, BUTTON_VOLUME_IRQ_LEVEL, BUTTON_IRQ_AUTO_FLIP },
};

#define BUTTON_COUNT 6

static const BUTTON_INTERRUPT_DATA gButtonInterruptData_2G[] =
{
	{ BUTTON_IPT2G_HOLD_ID,        BUTTON_IPT2G_HOLD_IRQ,        BUTTON_IRQ_TYPE, BUTTON_IPT2G_IRQ_LEVEL,  BUTTON_IRQ_AUTO_FLIP },
	{ BUTTON_IPT2G_HOME_ID,        BUTTON_IPT2G_HOME_IRQ,        BUTTON_IRQ_TYPE, BUTTON_IPT2G_IRQ_LEVEL,  BUTTON_IRQ_AUTO_FLIP },
	{ BUTTON_IPT2G_VOLUME_UP_ID,   BUTTON_IPT2G_VOLUME_UP_IRQ,   BUTTON_IRQ_TYPE, BUTTON_IPT2G_IRQ_LEVEL, BUTTON_IRQ_AUTO_FLIP },
	{ BUTTON_IPT2G_VOLUME_DOWN_ID, BUTTON_IPT2G_VOLUME_DOWN_IRQ, BUTTON_IRQ_TYPE, BUTTON_IPT2G_IRQ_LEVEL, BUTTON_IRQ_AUTO_FLIP },
};

#define BUTTON_COUNT_2G 4

#define MAX_BUTTON_COUNT 6

static struct
{
	KINTERRUPT Interrupt;
	KSPIN_LOCK InterruptSyncLock;
}
gButtonInterrupts[MAX_BUTTON_COUNT];

static KDPC gButtonDpc;

static void ButtonDpcRoutine(UNUSED PKDPC Dpc, UNUSED void* Context, void* SysArg1, void* SysArg2)
{
	int ButtonIndex = (int) (uintptr_t) SysArg1;
	int ButtonState = (int) (uintptr_t) SysArg2;
	
	//DbgPrint("Button %d %s.", ButtonIndex, ButtonState ? "Pressed" : "Released");
	LogMsg("Button %d %s.", ButtonIndex, ButtonState ? "Pressed" : "Released");
}

const BUTTON_INTERRUPT_DATA* UsedButtonData = gButtonInterruptData;

static void ButtonInterruptRoutine(UNUSED PKINTERRUPT Interrupt, void* Context)
{
	// TODO: bunch these up in a buffer so we don't lose anything
	uintptr_t ButtonIndex = (uintptr_t) Context;
	uintptr_t ButtonState = (uintptr_t) HalGetPinStateGpio(UsedButtonData[ButtonIndex].Id);
	
	KeEnqueueDpc(
		&gButtonDpc,
		(void*) ButtonIndex,
		(void*) ButtonState
	);
}

BSTATUS DriverEntry(PDRIVER_OBJECT DriverObject)
{
	gDriverObject = DriverObject;
	
	KeInitializeDpc(&gButtonDpc, ButtonDpcRoutine, NULL);
	
	int ButtonCount = BUTTON_COUNT;
	if (strcmp(LdrGetHalName(), "hals5l8720.sys") == 0) {
		UsedButtonData = gButtonInterruptData_2G;
		ButtonCount = BUTTON_COUNT_2G;
		LogMsg("Using iPod touch 2G button definitions");
	}
	
	for (int i = 0; i < ButtonCount; i++)
	{
		HalRegisterGpioInterrupt(
			UsedButtonData[i].Irq,
			UsedButtonData[i].Type,
			UsedButtonData[i].Level,
			UsedButtonData[i].AutoFlip
		);
		
		KeInitializeInterrupt(
			&gButtonInterrupts[i].Interrupt,
			ButtonInterruptRoutine,
			(void*) (uintptr_t) i,
			&gButtonInterrupts[i].InterruptSyncLock,
			APPLE_IRQ_NUMBER_GPIO_TO_NORMAL(UsedButtonData[i].Irq),
			IPL_DEVICES0, // ignored -- all GPIO interrupts are at IPL_DEVICES0 level
			false // Shared
		);
		
		KeConnectInterrupt(&gButtonInterrupts[i].Interrupt);
		
		HalEnableGpioInterrupt(UsedButtonData[i].Irq);
	}
	
	return STATUS_SUCCESS;
}
