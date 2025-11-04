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

enum
{
	// The fields are ordered from most to least significant.
	// For example, for ARGB8888, B is the LSB, and A is the MSB.
	//
	// A - Alpha (Ignored for the buffers themselves, used for blending)
	// R - Red
	// G - Green
	// B - Blue
	// I - Intensity (in the case of gray)
	// P - Palette
	COLOR_FORMAT_ARGB8888,
	COLOR_FORMAT_ABGR8888,
	COLOR_FORMAT_RGB888,
	COLOR_FORMAT_BGR888,
	COLOR_FORMAT_ARGB1555,
	COLOR_FORMAT_ABGR1555,
	COLOR_FORMAT_RGB565,
	COLOR_FORMAT_BGR565,
	COLOR_FORMAT_I8,
	COLOR_FORMAT_P8,
	COLOR_FORMAT_I4,
	COLOR_FORMAT_P4,
	COLOR_FORMAT_I1,
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
