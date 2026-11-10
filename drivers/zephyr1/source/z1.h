/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	z1.h

Abstract:
	This header provides common definitions for the zephyr1
	multitouch controller driver.

Licensing:
	Because this driver borrows significantly from OpeniBoot, this driver's
	source code is licensed separately under the GNU General Public License
	version 3.0.  You should have received a copy of the GNU General Public
	License along with this source code file, which is available at the
	`drivers/LICENSE.ARM` path.  Otherwise, view the web page containing the
	license at https://gnu.org/licenses/gpl.html.

Author:
	iProgramInCpp - 10 November 2026
***/
#pragma once

#include <ke.h>
#include <hal.h>
#include <ldr.h>
#include <ps.h>
#include <io.h>
#include <string.h>

#include "pins8900.h"

#define MT_GPIO_POWER        0x0804
#define MT_ATN_INTERRUPT     0xA3

#define MT_SPI_PORT 2
#define MT_SPI_CS   GPIO_SPI2_CS0_M68

// commands, presumably
#define MT_INFO_FAMILY_ID 0xD1
#define MT_INFO_SENSOR_INFO 0xD3
#define MT_INFO_SENSOR_REGION_DESC 0xD0
#define MT_INFO_SENSOR_REGION_PARAM 0xA1
#define MT_INFO_SENSOR_DIM 0xD9

typedef struct
{
	int Speed;
	int TxDelay;
	int RxDelay;
}
MT_SPI_SETTING, *PMT_SPI_SETTING;

extern MT_SPI_SETTING MtNormalSpeed, MtFastSpeed, MtReaderSpeed;

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
extern uint32_t MtCurNop;

extern uint32_t MtSensorWidth;
extern uint32_t MtSensorHeight;

extern volatile bool MtInitialized;

// Utility function. Reads the entire contents of a file into a buffer.
BSTATUS ReadEntireFile(const char* Path, void** OutBuffer, size_t* OutSize, uint32_t CreateOptions);

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
