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
	
	CGDrawRectangle(GraphicsContext, 0x4499FF, 150, 100, 300, 200);
	
	CGFillRectangleGradient(GraphicsContext, 0x4499FF, 0xFF9944, true,  500, 100, 300, 200);
	CGFillRectangleGradient(GraphicsContext, 0x4499FF, 0xFF9944, false, 500, 400, 300, 200);
	
	CGFillCircle(GraphicsContext, 0xFFCD12, 200, 300, 50);
	
	CGDrawLine(GraphicsContext, 0x81CBFF, 0, 0, GraphicsContext->Width, GraphicsContext->Height);
	CGDrawLine(GraphicsContext, 0xFFCB81, GraphicsContext->Width, 0, 0, GraphicsContext->Height);
}

BSTATUS SetupTerminal()
{
	DrawFrame();
	
	return STATUS_UNIMPLEMENTED;
}
