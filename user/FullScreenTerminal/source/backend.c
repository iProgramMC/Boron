#include "terminal.h"

#define MARGIN_H 80
#define MARGIN_V 60

#define TERM_TITLE_HEIGHT 23
#define TERM_BACKGROUND_COLOR RGB(85, 85, 85)
#define TERM_BORDER_COLOR RGB(0, 0, 0)
#define TERM_TITLE_COLOR RGB(0, 0, 0)
#define TERM_SHINE_A_COLOR RGB(170, 170, 170)
#define TERM_SHINE_B_COLOR RGB(85, 85, 85)

void DrawFrame()
{
	PGRAPHICS_CONTEXT Ctx = GraphicsContext;
	int Left, Right, Top, Bottom;
	
	Left = MARGIN_H;
	Right = Ctx->Width - MARGIN_H;
	
	if (Left > Right) {
		Left = 0;
		Right = Ctx->Width;
	}
	
	Top = MARGIN_V;
	Bottom = Ctx->Height - MARGIN_V;
	if (Top > Bottom) {
		Top = 0;
		Bottom = Ctx->Height;
	}
	
	// Fill a rectangle around the terminal window.
	CGFillRectangle(Ctx, TERM_BACKGROUND_COLOR, 0, 0, Ctx->Width, MARGIN_V);
	CGFillRectangle(Ctx, TERM_BACKGROUND_COLOR, 0, Ctx->Height - MARGIN_V, Ctx->Width, MARGIN_V);
	CGFillRectangle(Ctx, TERM_BACKGROUND_COLOR, 0, MARGIN_V, MARGIN_H, Ctx->Height - MARGIN_V * 2);
	CGFillRectangle(Ctx, TERM_BACKGROUND_COLOR, Right, MARGIN_V, MARGIN_H, Ctx->Height - MARGIN_V * 2);
	
	// Draw the window's borders.
	CGDrawRectangle(Ctx, TERM_BORDER_COLOR, Left - 1, Top - 1, Right - Left + 2, Bottom - Top + 2);
	CGDrawRectangle(Ctx, TERM_BORDER_COLOR, Left - 1, Top - 1 - TERM_TITLE_HEIGHT, Right - Left + 2, TERM_TITLE_HEIGHT + 1);
	
	// Draw the shine around the window's title.
	CGFillRectangle(Ctx, TERM_SHINE_A_COLOR, Left, Top - TERM_TITLE_HEIGHT, Right - Left, 1);
	CGFillRectangle(Ctx, TERM_SHINE_A_COLOR, Left, Top - TERM_TITLE_HEIGHT, 1, TERM_TITLE_HEIGHT - 1);
	CGFillRectangle(Ctx, TERM_SHINE_B_COLOR, Left, Top - 2, Right - Left, 1);
	CGFillRectangle(Ctx, TERM_SHINE_B_COLOR, Right - 1, Top - TERM_TITLE_HEIGHT, 1, TERM_TITLE_HEIGHT - 1);
	
	// Fill in the window title's rectangle.
	CGFillRectangle(Ctx, TERM_TITLE_COLOR, Left + 1, Top - TERM_TITLE_HEIGHT + 1, Right - Left - 2, TERM_TITLE_HEIGHT - 3);
}

BSTATUS SetupTerminal()
{
	DrawFrame();
	
	while (true)
		OSSleep(1000);
	
	return STATUS_UNIMPLEMENTED;
}
