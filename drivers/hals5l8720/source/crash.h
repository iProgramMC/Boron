/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ha/crash.h

Licensing:
	Because this driver borrows significantly from OpeniBoot, this driver's
	source code is licensed separately under the GNU General Public License
	version 3.0.  You should have received a copy of the GNU General Public
	License along with this source code file, which is available at the
	`drivers/LICENSE.ARM` path.  Otherwise, view the web page containing the
	license at https://gnu.org/licenses/gpl.html.

Author:
	iProgramInCpp - 2 July 2026
***/
#pragma once

#include <main.h>
#include "hali.h"

HAL_API
void HalProcessorCrashed();

NO_RETURN
void HalCrashSystem(const char* Message);
