/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	z2.h

Abstract:
	This header provides common definitions for the zephyr2
	multitouch controller driver.

Author:
	iProgramInCpp - 26 October 2026
***/
#pragma once

#include <ke.h>
#include <hal.h>
#include <ldr.h>
#include <ps.h>
#include <io.h>
#include <string.h>

#include "pins8900.h"
#include "pins8720.h"

// NOTE: The iPhone 2G uses zephyr1, not zephyr2.  Its definitions will either be removed
// from the z2 driver, or the two drivers will be merged together if their differences are
// minimal.

enum
{
	N82AP, // aka iPhone 3G
	N45AP, // aka iPod touch 1G
	N72AP, // aka iPod touch 2G
};

/*
From https://github.com/devos50/qemu-ios/blob/ipod_touch_[1g/2g]/hw/arm/ipod_touch_multitouch.c,
here is the only significant diff:

static void ipod_touch_multitouch_inform_frame_ready(IPodTouchMultitouchState *s) {
-    s->sysic->gpio_int_status[4] |= (1 << 27); // the multitouch interrupt bit is in group 4 (32 interrupts per group), and the 27th of the 4th group
-    qemu_irq_raise(s->sysic->gpio_irqs[4]);
+    s->sysic->gpio_int_status[3] |= (1 << 13); // the multitouch interrupt bit is in group 3 (32 interrupts per group), and the 13th of the 3th group
+    qemu_irq_raise(s->sysic->gpio_irqs[3]);
}

iPod touch 1:   IRQ grp #4, Int #27
iPod touch 2:   IRQ grp #3, Int #13

0x9B -> 0b10011011 -> 0b100, 0b11011 -> 4, 27

Reversing the process for the Touch 2:
3, 13 -> 0b011, 0b01101 -> 0b01101101 -> 0x6D
*/

#define MT_GPIO_UNKNOWN_606  0x0606  // TODO: Is this different on N72?
#define MT_GPIO_POWER        0x0701  // TODO: Is this different on N72?
#define MT_ATN_INTERRUPT     0x9B
#define MT_ATN_INTERRUPT_N72 0x6D

#define MT_SPI_N82   1
#define MT_SPI_N45   2
#define MT_SPI_N72   2

#define MT_SPI_CS_N82 GPIO_SPI1_CS0
#define MT_SPI_CS_N45 GPIO_SPI2_CS0_N45
#define MT_SPI_CS_N72 0  // TODO: GPIO_SPI4_CS0_N72

// commands, presumably
#define MT_INFO_FAMILY_ID 0xD1
#define MT_INFO_SENSOR_INFO 0xD3
#define MT_INFO_SENSOR_REGION_DESC 0xD0
#define MT_INFO_SENSOR_REGION_PARAM 0xA1
#define MT_INFO_SENSOR_DIM 0xD9

#define PROX_CALIBRATION_ADDRESS 0x400180
#define MT_CALIBRATION_ADDRESS   0x400200

int MtGpioPower();
int MtGpioUnknown606();
int MtAtnInterruptVector();
int MtSpiPort();
int MtSpiCsPin();

typedef struct
{
	int Speed;
	int TxDelay;
	int RxDelay;
}
MT_SPI_SETTING, *PMT_SPI_SETTING;

extern MT_SPI_SETTING MtNormalSpeed, MtFastSpeed;

int MtSpiTxRx(PMT_SPI_SETTING SpiSetting, const void* OutBuffer, size_t OutBufferLen, void* InBuffer, size_t InBufferLen);
int MtSpiTx(PMT_SPI_SETTING SpiSetting, const void* OutBuffer, size_t OutBufferLen);

typedef struct
{
	uint8_t FamilyId;

	// ...
}
PACKED
Z2_FAMILY_ID, *PZ2_FAMILY_ID;

typedef struct
{
	uint8_t Endianness;
	uint8_t SensorRows;
	uint8_t SensorColumns;
	uint8_t BcdVersionUpper;
	uint8_t BcdVersionLower;

	// ...
}
PACKED
Z2_SENSOR_INFO, *PZ2_SENSOR_INFO;

typedef struct
{
	uint32_t SensorWidthRaw;
	uint32_t SensorHeightRaw;

	// ...
}
PACKED
Z2_SENSOR_DIM, *PZ2_SENSOR_DIM;

extern int MtModel;

extern uint32_t MtInterfaceVersion;
extern uint32_t MtMaxPacketSize;
extern bool MtFlipNop;
extern uint32_t MtCurNop;

// I don't know what HBPP stands for, but this waits for the reply interrupt from
// the controller and returns whatever it sends back.
//
// These don't seem to be used except in the initialization procedure.
uint16_t MtPerformHbppAtnAck();
uint32_t MtPerformHbppLongAtnAck();

// Prepare the initialization thread for execution.  Once file systems are mounted,
// this will kick in and upload the firmware to the device.
bool MtSetupInitializationThread();

// Utility function. Reads the entire contents of a file into a buffer.
BSTATUS ReadEntireFile(const char* Path, void** OutBuffer, size_t* OutSize, uint32_t CreateOptions);
