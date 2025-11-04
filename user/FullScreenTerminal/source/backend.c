#include "terminal.h"

#define MARGIN_H 80
#define MARGIN_V 60

void FillRect(uint32_t Color, int X, int Y, int Width, int Height)
{
	size_t StartPitch = FbInfo.Pitch * Y + X * FbInfo.Bpp / 8, CurrentPitch = 0;
	for (int yi = 0; yi < Height; yi++)
	{
		uint8_t* Row = FbAddress + StartPitch + CurrentPitch;
		CurrentPitch += FbInfo.Pitch;
		
		switch (FbInfo.Bpp)
		{
			case 32:
			{
				uint32_t* Row32 = (uint32_t*) Row;
				for (int xi = 0; xi < Width; xi++)
					Row32[xi] = Color;
				
				break;
			}
			case 24:
			{
				for (int xi = 0; xi < Width * 3; xi += 3) {
					Row[xi + 0] = Color & 0xFF;
					Row[xi + 1] = (Color >> 8) & 0xFF;
					Row[xi + 2] = (Color >> 16) & 0xFF;
				}
				break;
			}
			case 16:
			{
				// Assuming BGRA5551 color
				uint16_t NewColor = 0;
				NewColor |= (Color & 0xFF) >> 3; // Blue
				NewColor |= (((Color >> 8) & 0xFF) >> 3) << 5; // Green
				NewColor |= (((Color >> 16) & 0xFF) >> 3) << 10; // Red
				
				uint16_t* Row16 = (uint16_t*) Row;
				for (int xi = 0; xi < Width; xi++) {
					Row16[xi] = NewColor;
				}
				break;
			}
			case 8:
			{
				// Assuming grayscale
				uint8_t NewColor = 0;
				NewColor += (Color & 0xFF) * 2126 / 10000 / 255;
				NewColor += ((Color >> 8) & 0xFF) * 7152 / 10000 / 255;
				NewColor += ((Color >> 16) & 0xFF) * 722 / 10000 / 255;
				
				for (int xi = 0; xi < Width; xi++) {
					Row[xi] = NewColor;
				}
				break;
			}
		}
	}
}

void DrawFrame()
{
	int Left, Right, Top, Bottom;
	
	Left = MARGIN_H;
	Right = FbInfo.Width - MARGIN_H;
	
	if (Left > Right) {
		Left = 0;
		Right = FbInfo.Width;
	}
	
	Top = MARGIN_V;
	Bottom = FbInfo.Height - MARGIN_V;
	if (Top > Bottom) {
		Top = 0;
		Bottom = FbInfo.Height;
	}
	
	FillRect(0xFF9944, Left, Top, Right - Left, Bottom - Top);
}

BSTATUS SetupTerminal()
{
	DrawFrame();
	
	return STATUS_UNIMPLEMENTED;
}
