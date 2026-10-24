/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	spi.c

Abstract:
	.

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
