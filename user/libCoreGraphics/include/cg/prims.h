// 4/11/2025 iProgramInCpp
#pragma once

#define RGB(r, g, b) ((((r) & 0xFF) << 16) | (((g) & 0xFF) << 8) | ((b) & 0xFF))

typedef struct _GRAPHICS_CONTEXT GRAPHICS_CONTEXT, *PGRAPHICS_CONTEXT;

// Fills a rectangle.
void CGFillRectangle(PGRAPHICS_CONTEXT Context, uint32_t Color, int X, int Y, int Width, int Height);

// Draws a line using Bresenham's line-drawing algorithm.
void CGDrawLine(PGRAPHICS_CONTEXT Context, uint32_t Color, int X1, int Y1, int X2, int Y2);

// Draws the contour of a rectangle.
void CGDrawRectangle(PGRAPHICS_CONTEXT Context, uint32_t Color, int X, int Y, int Width, int Height);

// Fills a rectangle with a linear gradient from one side to its opposite.
void CGFillRectangleGradient(
	PGRAPHICS_CONTEXT Context,
	uint32_t Color1,
	uint32_t Color2,
	bool IsLeftToRight,
	int X,
	int Y,
	int Width,
	int Height
);

// Fills a circle with a certain radius with its center at the specified coordinates.
void CGFillCircle(
	PGRAPHICS_CONTEXT GraphicsContext,
	uint32_t Color,
	int XCenter,
	int YCenter,
	int Radius
);