/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	test.c

Abstract:
	This module implements testing functions for the
	zephyr2 multitouch controller.

Author:
	iProgramInCpp - 27 October 2026
***/
#include "z2.h"

PLOADER_FRAMEBUFFER Framebuffer = NULL;
uint8_t* FramebufferAddress = NULL;

static inline ALWAYS_INLINE UNUSED int Min(int a, int b) {
	return a < b ? a : b;
}
static inline ALWAYS_INLINE UNUSED int Max(int a, int b) {
	return a > b ? a : b;
}
static inline ALWAYS_INLINE UNUSED int Abs(int x) {
	return x < 0 ? -x : x;
}

uint32_t MtConvertColorToNative(uint32_t Color)
{
	uint32_t Result = 0;

	uint8_t Red = (Color >> 16) & 0xFF;
	uint8_t Green = (Color >> 8) & 0xFF;
	uint8_t Blue = Color & 0xFF;

	if (Framebuffer->RedMaskSize < 8)
		Red >>= 8 - Framebuffer->RedMaskSize;
	if (Framebuffer->GreenMaskSize < 8)
		Green >>= 8 - Framebuffer->GreenMaskSize;
	if (Framebuffer->BlueMaskSize < 8)
		Blue >>= 8 - Framebuffer->BlueMaskSize;

	Result |= Red << Framebuffer->RedMaskShift;
	Result |= Green << Framebuffer->GreenMaskShift;
	Result |= Blue << Framebuffer->BlueMaskShift;

	return Result;
}

bool MtClipCoordinates(int* X, int* Y, int* Width, int* Height)
{
	if (!Framebuffer)
		return false;

	// Ensure no boundaries are breached.
	if (!*Width || !*Height) return false;
	if (*X >= (int) Framebuffer->Width) return false;
	if (*Y >= (int) Framebuffer->Height) return false;
	if (*X + (int) Framebuffer->Width < 0) return false;
	if (*Y + (int) Framebuffer->Height < 0) return false;

	// Cap the coordinates off.
	if (*X + *Width > (int) Framebuffer->Width)
		*Width = (int) Framebuffer->Width - *X;
	if (*Y + *Height > (int) Framebuffer->Height)
		*Y = (int) Framebuffer->Height - *Height;

	if (*X < 0) {
		*Width -= *X;
		*X = 0;
	}

	if (*Y < 0) {
		*Height -= *Y;
		*Y = 0;
	}

	return *Width > 0 && *Height > 0;
}

static void MtSetPixelNoBoundCheck(uint32_t NativeColor, int X, int Y)
{
	switch (Framebuffer->BitDepth)
	{
		case 32:
		{
			uint32_t* Address32 = (uint32_t*)(FramebufferAddress + Framebuffer->Pitch * Y + X * 4);
			*Address32 = NativeColor;
			break;
		}
		case 24:
		{
			uint8_t* Address24 = FramebufferAddress + Framebuffer->Pitch * Y + X * 3;
			*((uint16_t*)Address24) = NativeColor & 0xFFFF;
			Address24[2] = (NativeColor >> 8) & 0xFF;
			break;
		}
		case 16:
		{
			uint16_t* Address16 = (uint16_t*)(FramebufferAddress + Framebuffer->Pitch * Y + X * 2);
			*Address16 = NativeColor;
			break;
		}
		case 8:
		{
			uint8_t* Address8 = FramebufferAddress + Framebuffer->Pitch * Y + X;
			*Address8 = NativeColor;
			break;
		}
		case 4:
		{
			uint8_t* Address4 = FramebufferAddress + Framebuffer->Pitch * Y + (X >> 1);

			if (X & 1)
				*Address4 = (*Address4 & 0x0F) | (NativeColor << 4);
			else
				*Address4 = (*Address4 & 0xF0) | (NativeColor & 0xF);

			break;
		}
		case 1:
		{
			uint8_t* Address = FramebufferAddress + Framebuffer->Pitch * Y + (X >> 3);
			*Address = (*Address & ~(1 << (X & 7))) | (NativeColor ? (1 << (X & 7)) : 0);
			break;
		}
		default:
		{
			DbgPrint("%s: Unsupported bit depth %d.", __func__, Framebuffer->BitDepth);
			break;
		}
	}
}

static void MtSetPixel(uint32_t NativeColor, int X, int Y)
{
	if (!Framebuffer)
		return;
	if (X < 0 || Y < 0 || X >= (int) Framebuffer->Width || Y >= (int) Framebuffer->Height)
		return;

	MtSetPixelNoBoundCheck(NativeColor, X, Y);
}

void MtFillRectangle(uint32_t Color, int X, int Y, int Width, int Height)
{
	if (!Framebuffer)
		return;
	if (!MtClipCoordinates(&X, &Y, &Width, &Height))
		return;

	uint32_t NativeColor = MtConvertColorToNative(Color);

	size_t StartPitch = Framebuffer->Pitch * Y + X * (Framebuffer->BitDepth / 8), CurrentPitch = 0;
	for (int yi = 0; yi < Height; yi++)
	{
		uint8_t* Row = FramebufferAddress + StartPitch + CurrentPitch;
		CurrentPitch += Framebuffer->Pitch;

		switch (Framebuffer->BitDepth)
		{
			case 32:
			{
				uint32_t* Row32 = (uint32_t*) Row;
				for (int xi = 0; xi < Width; xi++)
					Row32[xi] = NativeColor;

				break;
			}
			case 24:
			{
				for (int xi = 0; xi < Width * 3; xi += 3) {
					*(uint16_t*) &Row[xi + 0] = NativeColor & 0xFFFF;
					Row[xi + 2] = (NativeColor >> 16) & 0xFF;
				}
				break;
			}
			case 16:
			{
				uint16_t* Row16 = (uint16_t*) Row;
				for (int xi = 0; xi < Width; xi++) {
					Row16[xi] = (uint16_t) NativeColor;
				}
				break;
			}
			case 8:
			{
				for (int xi = 0; xi < Width; xi++) {
					Row[xi] = (uint8_t) NativeColor;
				}
				break;
			}
			case 4:
			{
				// TODO: Optimize.  We can set more pixels at once.
				for (int xi = 0; xi < Width; xi++) {
					if (xi & 1)
						Row[xi >> 1] = (Row[xi >> 1] & 0x0F) | (NativeColor << 4);
					else
						Row[xi >> 1] = (Row[xi >> 1] & 0xF0) | (NativeColor & 0x0F);
				}
				break;
			}
			case 1:
			{
				// TODO: Optimize.  We can set more pixels at once.
				for (int xi = 0; xi < Width; xi++) {
					uint8_t Data = Row[xi >> 3];
					Data &= ~(1 << (xi & 7));
					if (NativeColor)
						Data |= 1 << (xi & 7);
					Row[xi >> 3] = Data;
				}
				break;
			}
			default:
			{
				DbgPrint("%s: Unsupported bit depth %d.", __func__, Framebuffer->BitDepth);
				break;
			}
		}
	}
}

void MtDrawLine(uint32_t Color, int X1, int Y1, int X2, int Y2)
{
	if (X1 == X2)
	{
		// Draw a vertical line.
		MtFillRectangle(Color, X1, Min(Y1, Y2), 1, Abs(Y1 - Y2) + 1);
		return;
	}

	if (Y1 == Y2)
	{
		// Draw a horizontal line.
		MtFillRectangle(Color, Min(X1, X2), Y1, Abs(X1 - X2) + 1, 1);
		return;
	}

	uint32_t NativeColor = MtConvertColorToNative(Color);

	// Use Bresenham's line algorithm to draw a line.
	int Dx = X2 - X1, Dy = Y2 - Y1;
	int Dx1 = Abs(Dx), Dy1 = Abs(Dy), Xe, Ye, X, Y;
	int Px = 2 * Dy1 - Dx1, Py = 2 * Dx1 - Dy1;

	if (Dy1 <= Dx1)
	{
		if (Dx >= 0)
			X = X1, Y = Y1, Xe = X2;
		else
			X = X2, Y = Y2, Xe = X1;

		MtSetPixel(NativeColor, X, Y);

		for (int i = 0; X < Xe; i++)
		{
			(void) i;
			X++;
			if (Px < 0)
			{
				Px += 2 * Dy1;
			}
			else
			{
				if ((Dx < 0 && Dy < 0) || (Dx > 0 && Dy > 0))
					Y++;
				else
					Y--;

				Px += 2 * (Dy1 - Dx1);
			}

			MtSetPixel(NativeColor, X, Y);
		}
	}
	else
	{
		if (Dy >= 0)
			Y = Y1, X = X1, Ye = Y2;
		else
			Y = Y2, X = X2, Ye = Y1;

		MtSetPixel(NativeColor, X, Y);

		for (int i = 0; Y < Ye; i++)
		{
			(void) i;
			Y++;
			if (Py < 0)
			{
				Py += 2 * Dx1;
			}
			else
			{
				if ((Dx < 0 && Dy < 0) || (Dx > 0 && Dy > 0))
					X++;
				else
					X--;

				Py += 2 * (Dx1 - Dy1);
			}

			MtSetPixel(NativeColor, X, Y);
		}
	}
}

void MtDrawRectangle(uint32_t Color, int X, int Y, int Width, int Height)
{
	MtDrawLine(Color, X, Y, X + Width - 1, Y);
	MtDrawLine(Color, X, Y + Height - 1, X + Width - 1, Y + Height - 1);
	MtDrawLine(Color, X, Y, X, Y + Height - 1);
	MtDrawLine(Color, X + Width - 1, Y, X + Width - 1, Y + Height - 1);
}

int MtFramebufferWidth()
{
	return Framebuffer ? Framebuffer->Width : 0;
}

int MtFramebufferHeight()
{
	return Framebuffer ? Framebuffer->Height : 0;
}

void MtInitializeTestDisplay()
{
	Framebuffer = NULL;
	if (KeLoaderParameterBlock.FramebufferCount == 0)
	{
		DbgPrint("ERROR: No framebuffers.");
	}
	else
	{
		Framebuffer = &KeLoaderParameterBlock.Framebuffers[0];

		FramebufferAddress = MmMapIoSpace(
			(uintptr_t) Framebuffer->Address,
			Framebuffer->Pitch * Framebuffer->Height,
			MM_PROT_READ | MM_PROT_WRITE | MM_MISC_DISABLE_CACHE,
			POOL_TAG("TSFB")
		);

		ASSERT(FramebufferAddress && "Out of memory");
	}
}
