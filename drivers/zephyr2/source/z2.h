/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	z2.h

Abstract:
	This header provides common definitions for the zephyr2
	multitouch controller driver.

Licensing:
	Because this driver borrows significantly from OpeniBoot, this driver's
	source code is licensed separately under the GNU General Public License
	version 3.0.  You should have received a copy of the GNU General Public
	License along with this source code file, which is available at the
	`drivers/LICENSE.ARM` path.  Otherwise, view the web page containing the
	license at https://gnu.org/licenses/gpl.html.

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

#define MT_GPIO_UNKNOWN_606  0x0606
#define MT_GPIO_POWER        0x0701
#define MT_ATN_INTERRUPT     0x9B
#define MT_ATN_INTERRUPT_N72 0x6D

#define MT_GPIO_UNKNOWN_606_N72 0x0a06
#define MT_GPIO_POWER_N72       0x0a06

#define MT_SPI_N82   1
#define MT_SPI_N45   2
#define MT_SPI_N72   4

#define MT_SPI_CS_N82 GPIO_SPI1_CS0
#define MT_SPI_CS_N45 GPIO_SPI2_CS0_N45
#define MT_SPI_CS_N72 GPIO_SPI4_CS0_N72

// commands, presumably
#define MT_INFO_FAMILY_ID 0xD1
#define MT_INFO_SENSOR_INFO 0xD3
#define MT_INFO_SENSOR_REGION_DESC 0xD0
#define MT_INFO_SENSOR_REGION_PARAM 0xA1
#define MT_INFO_SENSOR_DIM 0xD9

#define PROX_CALIBRATION_ADDRESS 0x400180
#define MT_CALIBRATION_ADDRESS   0x400200

int MtGpioPower();
int MtGpioResetPin();
int MtAtnInterruptVector();
int MtSpiPort();
int MtSpiCsPin();
int MtFirmwareSuccessfulResponse();
int MtGpioResetAsserted();

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

typedef struct
{
	uint8_t Type;
	uint8_t FrameNum;
	uint8_t HeaderLength;
	uint8_t Unk3;
	uint32_t Timestamp;
	uint8_t Unk8;
	uint8_t Unk9;
	uint8_t UnkA;
	uint8_t UnkB;
	uint16_t UnkC;
	uint16_t IsImage;
	uint8_t FingerCount;
	uint8_t FingerDataLength;
	uint16_t Unk12;
	uint16_t Unk14;
	uint16_t Unk16;
}
PACKED
MT_FRAME_HEADER, *PMT_FRAME_HEADER;

typedef struct
{
	uint8_t Id;
	uint8_t Event;
	uint8_t Unk2;
	uint8_t Unk3;
	int16_t X;
	int16_t Y;
	int16_t VelX;
	int16_t VelY;
	uint16_t Radius2;
	uint16_t Radius3;
	uint16_t Angle;
	uint16_t Radius1;
	uint16_t ContactDensity;
	uint16_t Unk16;
	uint16_t Unk18;
	uint16_t Unk1A;
}
PACKED
MT_FINGER_DATA, *PMT_FINGER_DATA;

extern int MtModel;

extern uint32_t MtInterfaceVersion;
extern uint32_t MtMaxPacketSize;
extern bool MtFlipNop;
extern uint32_t MtCurNop;

extern uint32_t MtSensorWidth;
extern uint32_t MtSensorHeight;

extern volatile bool MtInitialized;
extern volatile bool MtEmulatorMode;

// I don't know what HBPP stands for, but this waits for the reply interrupt from
// the controller and returns whatever it sends back.
//
// These don't seem to be used except in the initialization procedure.
uint16_t MtPerformHbppAtnAck();
uint32_t MtPerformHbppLongAtnAck();

// Utility function. Reads the entire contents of a file into a buffer.
BSTATUS ReadEntireFile(const char* Path, void** OutBuffer, size_t* OutSize, uint32_t CreateOptions);

uint16_t MtChecksum(uint8_t* Buffer, size_t Start, size_t End);
uint32_t MtChecksum32(uint8_t* Buffer, size_t Start, size_t End);

bool MtPrepareInitialData();
bool MtSetupMultitouchDevice();
void MtRunMultitouch();

bool MtCheckForEvents(bool Wait, int TimeoutMS);

// Test
void MtFillRectangle(uint32_t Color, int X, int Y, int Width, int Height);
void MtDrawLine(uint32_t Color, int X1, int Y1, int X2, int Y2);
void MtDrawRectangle(uint32_t Color, int X, int Y, int Width, int Height);
int MtFramebufferWidth();
int MtFramebufferHeight();
void MtInitializeTestDisplay();
