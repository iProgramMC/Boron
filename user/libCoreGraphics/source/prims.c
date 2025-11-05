// 4/11/2025 iProgramInCpp
#include <boron.h>
#include <cg/context.h>
#include <cg/color.h>
#include <cg/prims.h>

static inline ALWAYS_INLINE int Min(int a, int b) {
	return a < b ? a : b;
}
static inline ALWAYS_INLINE int Max(int a, int b) {
	return a > b ? a : b;
}
static inline ALWAYS_INLINE int Abs(int x) {
	return x < 0 ? -x : x;
}

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

static void CGSetPixelNoBoundCheck(PGRAPHICS_CONTEXT Context, uint32_t NativeColor, int X, int Y)
{
	switch (Context->Bpp)
	{
		case 32:
		{
			uint32_t* Address32 = (uint32_t*)(Context->BufferAddress + Context->Pitch * Y + X * 4);
			*Address32 = NativeColor;
			break;
		}
		case 24:
		{
			uint8_t* Address24 = Context->BufferAddress + Context->Pitch * Y + X * 3;
			*((uint16_t*)Address24) = NativeColor & 0xFFFF;
			Address24[2] = (NativeColor >> 8) & 0xFF;
			break;
		}
		case 16:
		{
			uint16_t* Address16 = (uint16_t*)(Context->BufferAddress + Context->Pitch * Y + X * 2);
			*Address16 = NativeColor;
			break;
		}
		case 8:
		{
			uint8_t* Address8 = Context->BufferAddress + Context->Pitch * Y + X;
			*Address8 = NativeColor;
			break;
		}
		case 4:
		{
			uint8_t* Address4 = Context->BufferAddress + Context->Pitch * Y + (X >> 1);
			
			if (X & 1)
				*Address4 = (*Address4 & 0x0F) | (NativeColor << 4);
			else
				*Address4 = (*Address4 & 0xF0) | (NativeColor & 0xF);
			
			break;
		}
		case 1:
		{
			uint8_t* Address = Context->BufferAddress + Context->Pitch * Y + (X >> 3);
			*Address = (*Address & ~(1 << (X & 7))) | (NativeColor ? (1 << (X & 7)) : 0);
			break;
		}
		default:
		{
			DbgPrint("%s: Unsupported bit depth %d.", __func__, Context->Bpp);
			break;
		}
	}
}

static void CGSetPixel(PGRAPHICS_CONTEXT Context, uint32_t NativeColor, int X, int Y)
{
	if (X < 0 || Y < 0 || X >= Context->Width || Y >= Context->Height)
		return;
	
	CGSetPixelNoBoundCheck(Context, NativeColor, X, Y);
}

void CGFillRectangle(PGRAPHICS_CONTEXT Context, uint32_t Color, int X, int Y, int Width, int Height)
{
	if (!CGClipCoordinates(Context, &X, &Y, &Width, &Height))
		return;
	
	uint32_t NativeColor = CGConvertColorToNative(Context, Color);
	
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
				DbgPrint("%s: Unsupported bit depth %d.", __func__, Context->Bpp);
				break;
			}
		}
	}
}

void CGDrawLine(PGRAPHICS_CONTEXT Context, uint32_t Color, int X1, int Y1, int X2, int Y2)
{
	if (X1 == X2)
	{
		// Draw a vertical line.
		CGFillRectangle(Context, Color, X1, Min(Y1, Y2), 1, Abs(Y1 - Y2) + 1);
		return;
	}
	
	if (Y1 == Y2)
	{
		// Draw a horizontal line.
		CGFillRectangle(Context, Color, Min(X1, X2), Y1, Abs(X1 - X2) + 1, 1);
		return;
	}
	
	uint32_t NativeColor = CGConvertColorToNative(Context, Color);
	
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
		
		CGSetPixel(Context, NativeColor, X, Y);
		
		for (int i = 0; X < Xe; i++)
		{
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
			
			CGSetPixel(Context, NativeColor, X, Y);
		}
	}
	else
	{
		if (Dy >= 0)
			Y = Y1, X = X1, Ye = Y2;
		else
			Y = Y2, X = X2, Ye = Y1;
		
		CGSetPixel(Context, NativeColor, X, Y);
		
		for (int i = 0; Y < Ye; i++)
		{
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
				
				Py += 2 * (Dy1 - Dx1);
			}
			
			CGSetPixel(Context, NativeColor, X, Y);
		}
	}
	
}

void CGDrawRectangle(PGRAPHICS_CONTEXT Context, uint32_t Color, int X, int Y, int Width, int Height)
{
	CGDrawLine(Context, Color, X, Y, X + Width - 1, Y);
	CGDrawLine(Context, Color, X, Y + Height - 1, X + Width - 1, Y + Height - 1);
	CGDrawLine(Context, Color, X, Y, X, Y + Height - 1);
	CGDrawLine(Context, Color, X + Width - 1, Y, X + Width - 1, Y + Height - 1);
}

void CGFillRectangleGradient(
	PGRAPHICS_CONTEXT Context,
	uint32_t Color1,
	uint32_t Color2,
	bool IsLeftToRight,
	int X,
	int Y,
	int Width,
	int Height
)
{
	if (Width == 0 || Height == 0)
		return;
	
	COLOR_ARGB8888 CGColor1, CGColor2;
	CGColor1.n = Color1;
	CGColor2.n = Color2;
	
	if (IsLeftToRight)
	{
		for (int xi = 0; xi < Width; xi++)
		{
			COLOR_ARGB8888 Blended;
			Blended.r = (CGColor1.r * xi + CGColor2.r * (Width - xi)) / Width;
			Blended.g = (CGColor1.g * xi + CGColor2.g * (Width - xi)) / Width;
			Blended.b = (CGColor1.b * xi + CGColor2.b * (Width - xi)) / Width;
			Blended.a = (CGColor1.a * xi + CGColor2.a * (Width - xi)) / Width;
			
			CGDrawLine(Context, Blended.n, X + xi, Y, X + xi, Y + Height - 1);
		}
	}
	else
	{
		for (int yi = 0; yi < Height; yi++)
		{
			COLOR_ARGB8888 Blended;
			Blended.r = (CGColor1.r * yi + CGColor2.r * (Height - yi)) / Height;
			Blended.g = (CGColor1.g * yi + CGColor2.g * (Height - yi)) / Height;
			Blended.b = (CGColor1.b * yi + CGColor2.b * (Height - yi)) / Height;
			Blended.a = (CGColor1.a * yi + CGColor2.a * (Height - yi)) / Height;
			
			CGDrawLine(Context, Blended.n, X, Y + yi, X + Width - 1, Y + yi);
		}
	}
}

void CGFillCircle(PGRAPHICS_CONTEXT GraphicsContext, uint32_t Color, int XCenter, int YCenter, int Radius)
{
	if (Radius < 0) return;
	if (Radius == 0)
	{
		CGSetPixel(GraphicsContext, Color, XCenter, YCenter);
		return;
	}
	
	int x = XCenter, y = YCenter;
	int x0 = 0;
	int y0 = Radius;
	int d = 3 - 2 * Radius;
	
	while (y0 >= x0)
	{
		CGDrawLine(GraphicsContext, Color, x - y0, y - x0, x + y0, y - x0);
		if (x0 > 0)
			CGDrawLine(GraphicsContext, Color, x - y0, y + x0, x + y0, y + x0);
		
		if (d < 0)
		{
			d += 4 * x0++ + 6;
		}
		else
		{
			if (x0 != y0)
			{
				CGDrawLine(GraphicsContext, Color, x - x0, y - y0, x + x0, y - y0);
				CGDrawLine(GraphicsContext, Color, x - x0, y + y0, x + x0, y + y0);
			}
			d += 4 * (x0++ - y0--) + 10;
		}
	}
}
