#include "terminal.h"

#define MARGIN_H 80
#define MARGIN_V 60

void DrawFrame()
{
	int Left, Right, Top, Bottom;
	
	Left = MARGIN_H;
	Right = GraphicsContext->Width - MARGIN_H;
	
	if (Left > Right) {
		Left = 0;
		Right = GraphicsContext->Width;
	}
	
	Top = MARGIN_V;
	Bottom = GraphicsContext->Height - MARGIN_V;
	if (Top > Bottom) {
		Top = 0;
		Bottom = GraphicsContext->Height;
	}
	
	CGFillRectangle(GraphicsContext, 0xFF9944, Left, Top, Right - Left, Bottom - Top);
}

BSTATUS SetupTerminal()
{
	DrawFrame();
	
	return STATUS_UNIMPLEMENTED;
}
