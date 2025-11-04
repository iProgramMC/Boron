/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ioctl.h
	
Abstract:
	This header file contains I/O control structures
	and code definitions device files.
	
Author:
	iProgramInCpp - 8 October 2025
***/
#pragma once

enum
{
	// Framebuffer
	IOCTL_FRAMEBUFFER_GET_INFO = 0x10000,
};

typedef struct
{
	uint32_t Width;
	uint32_t Height;
	uint32_t Pitch;
	short    Bpp;
	short    ColorFormat;
}
IOCTL_FRAMEBUFFER_INFO, *PIOCTL_FRAMEBUFFER_INFO;
