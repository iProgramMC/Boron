/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ha/font.h
	
Abstract:
	This header file contains the Tamsyn 8x16 bitmap font.
	Copyright (C) 2015 Scott Fial
	http://www.fial.com/~scott/tamsyn-font/
	
Author:
	iProgramInCpp - 28 August 2023
***/

static uint8_t HalpBuiltInFont[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x38,0x44,0x44,0x44,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF8,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0xFF,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x0F,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0xF8,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x1C,0x22,0x20,0x20,0xF8,0x20,0x20,0x72,0x8C,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x10,0x10,0x10,0x10,0x10,0x10,0x00,0x00,0x10,0x10,0x00,0x00,0x00,0x00,
0x00,0x24,0x24,0x24,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x24,0x24,0x7E,0x24,0x24,0x24,0x7E,0x24,0x24,0x00,0x00,0x00,0x00,
0x00,0x00,0x08,0x08,0x1E,0x20,0x20,0x1C,0x02,0x02,0x3C,0x08,0x08,0x00,0x00,0x00,
0x00,0x00,0x00,0x30,0x49,0x4A,0x34,0x08,0x16,0x29,0x49,0x06,0x00,0x00,0x00,0x00,
0x00,0x00,0x30,0x48,0x48,0x48,0x30,0x31,0x49,0x46,0x46,0x39,0x00,0x00,0x00,0x00,
0x00,0x10,0x10,0x10,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x04,0x08,0x08,0x10,0x10,0x10,0x10,0x10,0x10,0x08,0x08,0x04,0x00,0x00,
0x00,0x00,0x20,0x10,0x10,0x08,0x08,0x08,0x08,0x08,0x08,0x10,0x10,0x20,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x24,0x18,0x7E,0x18,0x24,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x08,0x08,0x08,0x7F,0x08,0x08,0x08,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x08,0x08,0x10,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,
0x00,0x00,0x02,0x02,0x04,0x04,0x08,0x08,0x10,0x10,0x20,0x20,0x40,0x40,0x00,0x00,
0x00,0x00,0x00,0x3C,0x42,0x46,0x4A,0x52,0x62,0x42,0x42,0x3C,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x08,0x18,0x28,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x3C,0x42,0x02,0x02,0x04,0x08,0x10,0x20,0x7E,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x7E,0x04,0x08,0x1C,0x02,0x02,0x02,0x42,0x3C,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x04,0x0C,0x14,0x24,0x44,0x7E,0x04,0x04,0x04,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x7E,0x40,0x40,0x7C,0x02,0x02,0x02,0x42,0x3C,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x1C,0x20,0x40,0x40,0x7C,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x7E,0x02,0x04,0x04,0x08,0x08,0x10,0x10,0x10,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x3C,0x42,0x42,0x42,0x3C,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x3C,0x42,0x42,0x42,0x3E,0x02,0x02,0x04,0x38,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x08,0x08,0x10,0x00,
0x00,0x00,0x00,0x04,0x08,0x10,0x20,0x40,0x20,0x10,0x08,0x04,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x20,0x10,0x08,0x04,0x02,0x04,0x08,0x10,0x20,0x00,0x00,0x00,0x00,
0x00,0x00,0x3C,0x42,0x02,0x04,0x08,0x10,0x00,0x00,0x10,0x10,0x00,0x00,0x00,0x00,
0x00,0x00,0x1C,0x22,0x41,0x4F,0x51,0x51,0x51,0x53,0x4D,0x40,0x20,0x1F,0x00,0x00,
0x00,0x00,0x00,0x18,0x24,0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x7C,0x42,0x42,0x42,0x7C,0x42,0x42,0x42,0x7C,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x1E,0x20,0x40,0x40,0x40,0x40,0x40,0x20,0x1E,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x78,0x44,0x42,0x42,0x42,0x42,0x42,0x44,0x78,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x7E,0x40,0x40,0x40,0x7C,0x40,0x40,0x40,0x7E,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x7E,0x40,0x40,0x40,0x7C,0x40,0x40,0x40,0x40,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x1E,0x20,0x40,0x40,0x46,0x42,0x42,0x22,0x1E,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x42,0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x42,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x3E,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x02,0x02,0x02,0x02,0x02,0x02,0x42,0x42,0x3C,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x42,0x44,0x48,0x50,0x60,0x50,0x48,0x44,0x42,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x7E,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x41,0x63,0x55,0x49,0x49,0x41,0x41,0x41,0x41,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x42,0x42,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x3C,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x7C,0x42,0x42,0x42,0x7C,0x40,0x40,0x40,0x40,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x3C,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x04,0x02,0x00,0x00,
0x00,0x00,0x00,0x7C,0x42,0x42,0x42,0x7C,0x48,0x44,0x42,0x42,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x3E,0x40,0x40,0x20,0x18,0x04,0x02,0x02,0x7C,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x7F,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x42,0x42,0x42,0x42,0x42,0x24,0x24,0x18,0x18,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x41,0x41,0x41,0x41,0x49,0x49,0x49,0x55,0x63,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x41,0x41,0x22,0x14,0x08,0x14,0x22,0x41,0x41,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x41,0x41,0x22,0x14,0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x7E,0x04,0x08,0x08,0x10,0x10,0x20,0x20,0x7E,0x00,0x00,0x00,0x00,
0x00,0x00,0x1E,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x1E,0x00,0x00,
0x00,0x00,0x40,0x40,0x20,0x20,0x10,0x10,0x08,0x08,0x04,0x04,0x02,0x02,0x00,0x00,
0x00,0x00,0x78,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x78,0x00,0x00,
0x00,0x00,0x10,0x28,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,
0x00,0x20,0x10,0x08,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x3C,0x02,0x02,0x3E,0x42,0x42,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x40,0x40,0x40,0x7C,0x42,0x42,0x42,0x42,0x42,0x7C,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x3C,0x42,0x40,0x40,0x40,0x42,0x3C,0x00,0x00,0x00,0x00,
0x00,0x00,0x02,0x02,0x02,0x3E,0x42,0x42,0x42,0x42,0x42,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x3C,0x42,0x42,0x7E,0x40,0x40,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x0E,0x10,0x10,0x7E,0x10,0x10,0x10,0x10,0x10,0x10,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x3E,0x42,0x42,0x42,0x42,0x42,0x3E,0x02,0x02,0x3C,0x00,
0x00,0x00,0x40,0x40,0x40,0x7C,0x42,0x42,0x42,0x42,0x42,0x42,0x00,0x00,0x00,0x00,
0x00,0x00,0x08,0x08,0x00,0x38,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x04,0x04,0x00,0x1C,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x38,0x00,
0x00,0x00,0x40,0x40,0x40,0x44,0x48,0x50,0x70,0x48,0x44,0x42,0x00,0x00,0x00,0x00,
0x00,0x00,0x38,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x77,0x49,0x49,0x49,0x49,0x49,0x49,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x7C,0x42,0x42,0x42,0x42,0x42,0x42,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x7C,0x42,0x42,0x42,0x42,0x42,0x7C,0x40,0x40,0x40,0x00,
0x00,0x00,0x00,0x00,0x00,0x3E,0x42,0x42,0x42,0x42,0x42,0x3E,0x02,0x02,0x02,0x00,
0x00,0x00,0x00,0x00,0x00,0x2E,0x30,0x20,0x20,0x20,0x20,0x20,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x3E,0x40,0x20,0x18,0x04,0x02,0x7C,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x10,0x10,0x7E,0x10,0x10,0x10,0x10,0x10,0x0E,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x42,0x42,0x42,0x24,0x24,0x18,0x18,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x41,0x41,0x41,0x49,0x49,0x55,0x63,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x41,0x22,0x14,0x08,0x14,0x22,0x41,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x3E,0x02,0x02,0x3C,0x00,
0x00,0x00,0x00,0x00,0x00,0x7E,0x04,0x08,0x10,0x20,0x40,0x7E,0x00,0x00,0x00,0x00,
0x00,0x0E,0x10,0x10,0x10,0x10,0x10,0xE0,0x10,0x10,0x10,0x10,0x10,0x0E,0x00,0x00,
0x00,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x00,0x00,
0x00,0x70,0x08,0x08,0x08,0x08,0x08,0x07,0x08,0x08,0x08,0x08,0x08,0x70,0x00,0x00,
0x00,0x00,0x31,0x49,0x46,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x10,0x10,0x00,0x00,0x10,0x10,0x10,0x10,0x10,0x10,0x00,
0x00,0x00,0x08,0x08,0x1C,0x22,0x40,0x40,0x40,0x22,0x1C,0x08,0x08,0x00,0x00,0x00,
0x00,0x00,0x00,0x1C,0x22,0x20,0x20,0xF8,0x20,0x20,0x72,0x8C,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x42,0x3C,0x24,0x24,0x24,0x3C,0x42,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x41,0x22,0x14,0x08,0x3E,0x08,0x3E,0x08,0x08,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x08,0x08,0x08,0x08,0x00,0x00,0x00,0x08,0x08,0x08,0x08,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x22,0x22,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x1C,0x22,0x41,0x4D,0x51,0x51,0x4D,0x41,0x22,0x1C,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x09,0x12,0x24,0x48,0x24,0x12,0x09,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x38,0x44,0x44,0x44,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x08,0x30,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x48,0x24,0x12,0x09,0x12,0x24,0x48,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x08,0x08,0x00,0x00,0x08,0x10,0x20,0x40,0x42,0x3C,0x00,
0x20,0x10,0x00,0x18,0x18,0x24,0x24,0x24,0x7E,0x42,0x42,0x42,0x00,0x00,0x00,0x00,
0x04,0x08,0x00,0x18,0x18,0x24,0x24,0x24,0x7E,0x42,0x42,0x42,0x00,0x00,0x00,0x00,
0x18,0x24,0x00,0x18,0x18,0x24,0x24,0x24,0x7E,0x42,0x42,0x42,0x00,0x00,0x00,0x00,
0x32,0x4C,0x00,0x18,0x18,0x24,0x24,0x24,0x7E,0x42,0x42,0x42,0x00,0x00,0x00,0x00,
0x24,0x24,0x00,0x18,0x18,0x24,0x24,0x24,0x7E,0x42,0x42,0x42,0x00,0x00,0x00,0x00,
0x18,0x24,0x24,0x18,0x18,0x24,0x24,0x24,0x7E,0x42,0x42,0x42,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x0F,0x14,0x14,0x24,0x27,0x3C,0x44,0x44,0x47,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x1E,0x20,0x40,0x40,0x40,0x40,0x40,0x20,0x1E,0x08,0x08,0x30,0x00,
0x20,0x10,0x00,0x7E,0x40,0x40,0x40,0x7C,0x40,0x40,0x40,0x7E,0x00,0x00,0x00,0x00,
0x04,0x08,0x00,0x7E,0x40,0x40,0x40,0x7C,0x40,0x40,0x40,0x7E,0x00,0x00,0x00,0x00,
0x18,0x24,0x00,0x7E,0x40,0x40,0x40,0x7C,0x40,0x40,0x40,0x7E,0x00,0x00,0x00,0x00,
0x24,0x24,0x00,0x7E,0x40,0x40,0x40,0x7C,0x40,0x40,0x40,0x7E,0x00,0x00,0x00,0x00,
0x10,0x08,0x00,0x3E,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00,0x00,
0x04,0x08,0x00,0x3E,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00,0x00,
0x18,0x24,0x00,0x3E,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00,0x00,
0x22,0x22,0x00,0x3E,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x3C,0x22,0x21,0x21,0x79,0x21,0x21,0x22,0x3C,0x00,0x00,0x00,0x00,
0x32,0x4C,0x00,0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x42,0x42,0x00,0x00,0x00,0x00,
0x10,0x08,0x00,0x1C,0x22,0x41,0x41,0x41,0x41,0x41,0x22,0x1C,0x00,0x00,0x00,0x00,
0x04,0x08,0x00,0x1C,0x22,0x41,0x41,0x41,0x41,0x41,0x22,0x1C,0x00,0x00,0x00,0x00,
0x18,0x24,0x00,0x1C,0x22,0x41,0x41,0x41,0x41,0x41,0x22,0x1C,0x00,0x00,0x00,0x00,
0x32,0x4C,0x00,0x1C,0x22,0x41,0x41,0x41,0x41,0x41,0x22,0x1C,0x00,0x00,0x00,0x00,
0x24,0x24,0x00,0x1C,0x22,0x41,0x41,0x41,0x41,0x41,0x22,0x1C,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x22,0x14,0x08,0x14,0x22,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x02,0x3C,0x42,0x46,0x4A,0x52,0x62,0x42,0x42,0x3C,0x40,0x00,0x00,0x00,
0x20,0x10,0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00,
0x04,0x08,0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00,
0x18,0x24,0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00,
0x24,0x24,0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00,
0x04,0x08,0x00,0x41,0x41,0x22,0x14,0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x40,0x40,0x7C,0x42,0x42,0x42,0x7C,0x40,0x40,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x3C,0x42,0x44,0x4C,0x42,0x42,0x42,0x44,0x58,0x00,0x00,0x00,0x00,
0x00,0x00,0x20,0x10,0x00,0x3C,0x02,0x02,0x3E,0x42,0x42,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x04,0x08,0x00,0x3C,0x02,0x02,0x3E,0x42,0x42,0x3E,0x00,0x00,0x00,0x00,
0x00,0x18,0x24,0x00,0x00,0x3C,0x02,0x02,0x3E,0x42,0x42,0x3E,0x00,0x00,0x00,0x00,
0x00,0x32,0x4C,0x00,0x00,0x3C,0x02,0x02,0x3E,0x42,0x42,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x24,0x24,0x00,0x3C,0x02,0x02,0x3E,0x42,0x42,0x3E,0x00,0x00,0x00,0x00,
0x18,0x24,0x24,0x18,0x00,0x3C,0x02,0x02,0x3E,0x42,0x42,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x36,0x09,0x39,0x4F,0x48,0x48,0x37,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x3C,0x42,0x40,0x40,0x40,0x42,0x3C,0x08,0x08,0x30,0x00,
0x00,0x00,0x20,0x10,0x00,0x3C,0x42,0x42,0x7E,0x40,0x40,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x04,0x08,0x00,0x3C,0x42,0x42,0x7E,0x40,0x40,0x3E,0x00,0x00,0x00,0x00,
0x00,0x18,0x24,0x00,0x00,0x3C,0x42,0x42,0x7E,0x40,0x40,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x24,0x24,0x00,0x3C,0x42,0x42,0x7E,0x40,0x40,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x10,0x08,0x00,0x38,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x04,0x08,0x00,0x38,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00,0x00,
0x00,0x18,0x24,0x00,0x00,0x38,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x24,0x24,0x00,0x38,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00,0x00,
0x00,0x09,0x06,0x1A,0x01,0x1D,0x23,0x41,0x41,0x41,0x22,0x1C,0x00,0x00,0x00,0x00,
0x00,0x32,0x4C,0x00,0x00,0x7C,0x42,0x42,0x42,0x42,0x42,0x42,0x00,0x00,0x00,0x00,
0x00,0x00,0x10,0x08,0x00,0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00,
0x00,0x00,0x04,0x08,0x00,0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00,
0x00,0x18,0x24,0x00,0x00,0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00,
0x00,0x32,0x4C,0x00,0x00,0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00,
0x00,0x00,0x24,0x24,0x00,0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x7E,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x02,0x3C,0x46,0x4A,0x52,0x62,0x42,0x3C,0x40,0x00,0x00,0x00,
0x00,0x00,0x20,0x10,0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x04,0x08,0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x3E,0x00,0x00,0x00,0x00,
0x00,0x18,0x24,0x00,0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x24,0x24,0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x3E,0x00,0x00,0x00,0x00,
0x00,0x00,0x04,0x08,0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x3E,0x02,0x02,0x3C,0x00,
0x00,0x00,0x40,0x40,0x40,0x5C,0x62,0x41,0x41,0x41,0x62,0x5C,0x40,0x40,0x40,0x00,
0x00,0x00,0x24,0x24,0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x3E,0x02,0x02,0x3C,0x00,
};