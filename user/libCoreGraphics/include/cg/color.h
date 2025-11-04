// 4/11/2025 iProgramInCpp
#pragma once

#include <stdint.h>

typedef union { struct { uint8_t b, g, r, a; }; uint32_t n; } COLOR_ARGB8888;
typedef union { struct { uint8_t r, g, b, a; }; uint32_t n; } COLOR_ABGR8888;
typedef union { struct { uint8_t b, g, r; }; uint32_t n; } COLOR_RGB888;
typedef union { struct { uint8_t r, g, b; }; uint32_t n; } COLOR_BGR888;
typedef union { struct { uint8_t b : 5, g : 5, r : 5, a : 1; }; uint32_t n; } COLOR_ARGB1555;
typedef union { struct { uint8_t r : 5, g : 5, b : 5, a : 1; }; uint32_t n; } COLOR_ABGR1555;
typedef union { struct { uint8_t b : 5, g : 6, r : 5; }; uint32_t n; } COLOR_RGB565;
typedef union { struct { uint8_t r : 5, g : 6, b : 5; }; uint32_t n; } COLOR_BGR565;
typedef union { struct { uint8_t i; }; uint32_t n; } COLOR_I8;
typedef union { struct { uint8_t p; }; uint32_t n; } COLOR_P8;
typedef union { struct { uint8_t i : 4; }; uint32_t n; } COLOR_I4;
typedef union { struct { uint8_t p : 4; }; uint32_t n; } COLOR_P4;
typedef union { struct { uint8_t i : 1; }; uint32_t n; } COLOR_I1;

uint32_t CGConvertToNative(PGRAPHICS_CONTEXT Context, uint32_t Color);
