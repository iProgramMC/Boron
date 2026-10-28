/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	spi.c

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
#include "nor.h"

IO_DISPATCH_TABLE NorSpiDispatchTable;

bool NorSpiInit()
{
	LogMsg("SPI mode not supported for now.");
	return false;
}
