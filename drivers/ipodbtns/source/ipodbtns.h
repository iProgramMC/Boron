/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	device.c

Abstract:
	This header defines functions used throughout
	the ipodbtns driver.

Licensing:
	Because this driver borrows significantly from OpeniBoot, this driver's
	source code is licensed separately under the GNU General Public License
	version 3.0.  You should have received a copy of the GNU General Public
	License along with this source code file, which is available at the
	`drivers/LICENSE.ARM` path.  Otherwise, view the web page containing the
	license at https://gnu.org/licenses/gpl.html.

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
