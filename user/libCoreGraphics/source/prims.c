// 4/11/2025 iProgramInCpp
#include <boron.h>
#include <cg/context.h>
#include <cg/color.h>
#include <cg/prims.h>

bool CGClipCoordinates(PGRAPHICS_CONTEXT Context, int* X, int* Y, int* Width, int* Height)
{
	// Ensure no boundaries are breached.
	if (!*Width || !*Height) return false;
	if (*X >= Context->Width) return false;
	if (*Y >= Context->Height) return false;
	if (*X + Context->Width < 0) return false;
	if (*Y + Context->Height < 0) return false;
	
	// Cap the coordinates off.
	if (*X + *Width > Context->Width)
		*Width = Context->Width - *X;
	if (*Y + *Height > Context->Height)
		*Y = Context->Height - *Height;
	
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

void CGFillRectangle(PGRAPHICS_CONTEXT Context, uint32_t Color, int X, int Y, int Width, int Height)
{
	if (!CGClipCoordinates(Context, &X, &Y, &Width, &Height))
		return;
	
	uint32_t NativeColor = CGConvertToNative(Context, Color);
	
	size_t StartPitch = Context->Pitch * Y + X * (Context->Bpp / 8), CurrentPitch = 0;
	for (int yi = 0; yi < Height; yi++)
	{
		uint8_t* Row = Context->BufferAddress + StartPitch + CurrentPitch;
		CurrentPitch += Context->Pitch;
		
		switch (Context->Bpp)
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
					Row[xi + 0] = NativeColor & 0xFF;
					Row[xi + 1] = (NativeColor >> 8) & 0xFF;
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
						Row[xi] = (Row[xi] & 0xF0) | (NativeColor & 0x0F);
					else
						Row[xi] = (Row[xi] & 0x0F) | (NativeColor & 0xF0);
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
				DbgPrint("Unsupported bit depth %d.", Context->Bpp);
				break;
		}
	}
}