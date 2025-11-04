// 4/11/2025 iProgramInCpp
#pragma once

#define RGB(r, g, b) ((((r) & 0xFF) << 16) | (((g) & 0xFF) << 8) | ((b) & 0xFF))

typedef struct _GRAPHICS_CONTEXT GRAPHICS_CONTEXT, *PGRAPHICS_CONTEXT;

void CGFillRectangle(PGRAPHICS_CONTEXT Context, uint32_t Color, int X, int Y, int Width, int Height);
