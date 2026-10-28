/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	gpiopins.h

Abstract:
	This header defines pin numbers for the iPod/iPhone GPIO driver.

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

// NOTE: All pin definitions must be prefixed with M68AP, N45AP or N82AP to avoid
// conflicting with ../../hals5l8720/source/gpiopins.h.

/* GPIO Numbers */
#define GPIO_SPI0_CS0_M68 0x400
#define GPIO_SPI0_CS0_N45 0x700
#define GPIO_SPI0_CS0_N82 0x400 // seemingly!?

#define GPIO_SPI0_CS1_N82 0x705
#define GPIO_SPI0_CS2_N82 0x706

#define GPIO_SPI1_CS0     0x1800 // unchanged?

#define GPIO_SPI2_CS0_M68 0x705
#define GPIO_SPI2_CS0_N45 0x1804

#define GPIO_SPI2_CS1_N45 0x705
