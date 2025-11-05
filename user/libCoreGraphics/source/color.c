// 4/11/2025 iProgramInCpp
#include <boron.h>
#include <cg/context.h>
#include <cg/color.h>

uint32_t CGConvertColorToNative(PGRAPHICS_CONTEXT Context, uint32_t Color)
{
	COLOR_ARGB8888 CGColor;
	CGColor.n = Color;
	
	switch (Context->ColorFormat)
	{
		case COLOR_FORMAT_ARGB8888:
		case COLOR_FORMAT_RGB888:
			return Color;
		
		case COLOR_FORMAT_ABGR8888: {
			COLOR_ABGR8888 RColor;
			RColor.r = CGColor.r;
			RColor.g = CGColor.g;
			RColor.b = CGColor.b;
			RColor.a = CGColor.a;
			return RColor.n;
		}
		case COLOR_FORMAT_BGR888: {
			COLOR_BGR888 RColor;
			RColor.r = CGColor.r;
			RColor.g = CGColor.g;
			RColor.b = CGColor.b;
			return RColor.n;
		}
		case COLOR_FORMAT_ARGB1555: {
			COLOR_ARGB1555 RColor;
			RColor.r = CGColor.r >> 3;
			RColor.g = CGColor.g >> 3;
			RColor.b = CGColor.b >> 3;
			RColor.a = CGColor.a >= 128;
			return RColor.n;
		}
		case COLOR_FORMAT_ABGR1555: {
			COLOR_ABGR1555 RColor;
			RColor.r = CGColor.r >> 3;
			RColor.g = CGColor.g >> 3;
			RColor.b = CGColor.b >> 3;
			RColor.a = CGColor.a >= 128;
			return RColor.n;
		}
		case COLOR_FORMAT_RGB565: {
			COLOR_RGB565 RColor;
			RColor.r = CGColor.r >> 3;
			RColor.g = CGColor.g >> 2;
			RColor.b = CGColor.b >> 3;
			return RColor.n;
		}
		case COLOR_FORMAT_BGR565: {
			COLOR_BGR565 RColor;
			RColor.r = CGColor.r >> 3;
			RColor.g = CGColor.g >> 2;
			RColor.b = CGColor.b >> 3;
			return RColor.n;
		}
		case COLOR_FORMAT_P8: // TODO: look up in palette
		case COLOR_FORMAT_I8: {
			uint32_t Intensity = 0;
			Intensity += (Color & 0xFF) * 2126 / 10000 / 3;
			Intensity += ((Color >> 8) & 0xFF) * 7152 / 10000 / 3;
			Intensity += ((Color >> 16) & 0xFF) * 722 / 10000 / 3;
			return Intensity;
		}
		case COLOR_FORMAT_P4: // TODO: look up in palette
		case COLOR_FORMAT_I4: {
			uint32_t Intensity = 0;
			Intensity += (Color & 0xFF) * 2126 / 10000 / 3;
			Intensity += ((Color >> 8) & 0xFF) * 7152 / 10000 / 3;
			Intensity += ((Color >> 16) & 0xFF) * 722 / 10000 / 3;
			return Intensity >> 4;
		}
		case COLOR_FORMAT_I1: {
			uint32_t Intensity = 0;
			Intensity += (Color & 0xFF) * 2126 / 10000 / 3;
			Intensity += ((Color >> 8) & 0xFF) * 7152 / 10000 / 3;
			Intensity += ((Color >> 16) & 0xFF) * 722 / 10000 / 3;
			return Intensity >= 128;
		}
		default: {
			DbgPrint("Unrecognized color format %d.", Context->ColorFormat);
			return Color;
		}
	}
}

void CGGetColorFormatInfo(int ColorFormat, int* RMSz, int* GMSz, int* BMSz, int* RMSh, int* GMSh, int* BMSh)
{
#define U(rsz, gsz, bsz, rsh, gsh, bsh) (void)(*RMSz = rsz, *GMSz = gsz, *BMSz = bsz, *RMSh = rsh, *GMSh = gsh, *BMSh = bsh)
	switch (ColorFormat)
	{
		case COLOR_FORMAT_ARGB8888: return U(8, 8, 8, 16, 8, 0);
		case COLOR_FORMAT_ABGR8888: return U(8, 8, 8, 0, 8, 16);
		case COLOR_FORMAT_RGB888: return U(8, 8, 8, 16, 8, 0);
		case COLOR_FORMAT_BGR888: return U(8, 8, 8, 0, 8, 16);
		case COLOR_FORMAT_ARGB1555: return U(5, 5, 5, 10, 5, 0);
		case COLOR_FORMAT_ABGR1555: return U(5, 5, 5, 0, 5, 10);
		case COLOR_FORMAT_RGB565: return U(5, 6, 5, 11, 5, 0);
		case COLOR_FORMAT_BGR565: return U(5, 6, 5, 0, 5, 11);
		case COLOR_FORMAT_I8:
		case COLOR_FORMAT_I4:
		case COLOR_FORMAT_P8:
		case COLOR_FORMAT_P4:
		default: return U(0, 0, 0, 0, 0, 0);
	}
#undef U
}
