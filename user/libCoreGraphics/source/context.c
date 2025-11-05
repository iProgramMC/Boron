// 4/11/2025 iProgramInCpp
#include <boron.h>
#include <cg/context.h>

PGRAPHICS_CONTEXT CGCreateContextFromBuffer(
	void* BufferAddress,
	int Width,
	int Height,
	int Pitch,
	short Bpp,
	short ColorFormat
)
{
	PGRAPHICS_CONTEXT Context = OSAllocate(sizeof(GRAPHICS_CONTEXT));
	if (!Context)
		return NULL;
	
	Context->BufferAddress = BufferAddress;
	Context->Width = Width;
	Context->Height = Height;
	Context->Pitch = Pitch;
	Context->Bpp = Bpp;
	Context->ColorFormat = ColorFormat;
	
	return Context;
}

void CGFreeContext(PGRAPHICS_CONTEXT Context)
{
	OSFree(Context);
}

PGRAPHICS_CONTEXT CGCreateSubContext(
	PGRAPHICS_CONTEXT InitialContext,
	int X,
	int Y,
	int Width,
	int Height
)
{
	return CGCreateContextFromBuffer(
		InitialContext->BufferAddress + InitialContext->Pitch * Y + X * InitialContext->Bpp / 8,
		Width,
		Height,
		InitialContext->Pitch,
		InitialContext->Bpp,
		InitialContext->ColorFormat
	);
}
