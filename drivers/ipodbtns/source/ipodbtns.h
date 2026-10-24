/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	device.c

Abstract:
	This header defines functions used throughout
	the ipodbtns driver.

Author:
	iProgramInCpp - 24 October 2026
***/
#pragma once

#include <hal.h>
#include <io.h>
#include <ldr.h>
#include <string.h>

enum
{
	BTN_HOLD     = 0x00,
	BTN_HOME     = 0x01,
	BTN_VOL_UP   = 0x02,
	BTN_VOL_DOWN = 0x03,
	BTN_RINGER   = 0x04,
	BTN_RELEASED = 0x80,
};

void BtnsAddKeyToBuffer(uint8_t Code);

BSTATUS BtnsCreateDeviceObject();
