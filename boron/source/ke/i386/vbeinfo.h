/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	vbeinfo.h
	
Abstract:
	This header defines structures according to the VBE standard.
	
	These structures are used if Multiboot provided us with VBE information
	but *not* framebuffer information.
	
Author:
	iProgramInCpp - 1 May 2026
***/
#pragma once

typedef struct
{
	uint8_t  VbeSignature[4];
	uint16_t VbeVersion;
	uint16_t OemStringPtr[2];
	uint32_t Capabilities;
	uint16_t VideoModePtr[2];
	uint16_t TotalMemoryIn64KBBlocks;
	uint8_t  Reserved[492];
}
PACKED
VBE_INFO_BLOCK, *PVBE_INFO_BLOCK;

static_assert(sizeof(VBE_INFO_BLOCK) == 512);

typedef struct
{
	uint16_t Attributes;
	uint8_t  WindowA;
	uint8_t  WindowB;
	uint16_t Granularity;
	uint16_t WindowSize;
	uint16_t SegmentA;
	uint16_t SegmentB;
	uint32_t WindowFunctionPointer;
	uint16_t Pitch;
	uint16_t Width;
	uint16_t Height;
	uint8_t  WChar;
	uint8_t  YChar;
	uint8_t  Planes;
	uint8_t  Bpp;
	uint8_t  Banks;
	uint8_t  MemoryModel;
	uint8_t  BankSize;
	uint8_t  ImagePages;
	uint8_t  Reserved0;
	uint8_t  RedMask;
	uint8_t  RedPosition;
	uint8_t  GreenMask;
	uint8_t  GreenPosition;
	uint8_t  BlueMask;
	uint8_t  BluePosition;
	uint8_t  ReservedMask;
	uint8_t  ReservedPosition;
	uint8_t  DirectColorAttributes;
	uint32_t FrameBufferAddress;
	uint32_t OffScreenMemoryOffset;
	uint16_t OffScreenMemorySize;
	uint8_t  Reserved[206];
}
PACKED
VBE_MODE_INFO_BLOCK, *PVBE_MODE_INFO_BLOCK;

static_assert(sizeof(VBE_MODE_INFO_BLOCK) == 256);
