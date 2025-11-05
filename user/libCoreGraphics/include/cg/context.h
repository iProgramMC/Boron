// 4/11/2025 iProgramInCpp
#pragma once

#include <stdint.h>
#include <ioctl.h>

typedef struct _GRAPHICS_CONTEXT
{
	uint8_t* BufferAddress;
	int Width;
	int Height;
	int Pitch;
	short Bpp;
	short ColorFormat;
	
	// TODO: More data
}
GRAPHICS_CONTEXT, *PGRAPHICS_CONTEXT;

// Creates a graphics context from a frame buffer specification.
PGRAPHICS_CONTEXT CGCreateContextFromBuffer(
	void* BufferAddress,
	int Width,
	int Height,
	int Pitch,
	short Bpp,
	short ColorFormat
);

// Creates a graphics context using another graphics context as its source,
// with a specified offset.  Note that this DOES NOT perform any kind of
// boundary checking.
PGRAPHICS_CONTEXT CGCreateSubContext(
	PGRAPHICS_CONTEXT InitialContext,
	int X,
	int Y,
	int Width,
	int Height
);

void CGFreeContext(PGRAPHICS_CONTEXT Context);
