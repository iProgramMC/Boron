/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ioctl.h

Abstract:
	This header file contains I/O control structures
	and code definitions device files.

Author:
	iProgramInCpp - 8 October 2025
***/
#pragma once

#include <stdint.h>

#define IOCTL_CODE(Class, Code) (((Class) << 16) | (Code))

enum
{
	IOCTL_CLASS_NONE = 0,
	IOCTL_CLASS_FRAMEBUFFER,
	IOCTL_CLASS_TERMINAL,
	IOCTL_CLASS_KEYBOARD,
	IOCTL_CLASS_POINTING_DEVICE,
	IOCTL_CLASS_VIDEO_INPUT,
	IOCTL_CLASS_BEEPER,
	IOCTL_CLASS_WAVE_OUTPUT,
};

enum
{
	// Framebuffer
	IOCTL_FRAMEBUFFER_GET_INFO = IOCTL_CODE(IOCTL_CLASS_FRAMEBUFFER, 0),
	IOCTL_FRAMEBUFFER_FLUSH = IOCTL_CODE(IOCTL_CLASS_FRAMEBUFFER, 1),

	// Terminal (see ttys.h for structures)
	IOCTL_TERMINAL_GET_TERMINAL_STATE = IOCTL_CODE(IOCTL_CLASS_TERMINAL, 0),
	IOCTL_TERMINAL_SET_TERMINAL_STATE = IOCTL_CODE(IOCTL_CLASS_TERMINAL, 1),
	IOCTL_TERMINAL_GET_WINDOW_SIZE = IOCTL_CODE(IOCTL_CLASS_TERMINAL, 2),
	IOCTL_TERMINAL_SET_WINDOW_SIZE = IOCTL_CODE(IOCTL_CLASS_TERMINAL, 3),

	// Keyboard (TODO)

	// Pointing device (TODO)

	// Video input device (TODO)

	// Beeper device (TODO)

	// Wave output device (TODO)
};

//
// Framebuffer devices DO NOT expose read/write primitives.  I/O is performed exclusively
// through the following I/O control codes, as well as through memory mapped I/O.
//

enum
{
	// The fields are ordered from most to least significant.
	// For example, for ARGB8888, B is the LSB, and A is the MSB.
	//
	// A - Alpha (Ignored for the buffers themselves, used for blending)
	// R - Red
	// G - Green
	// B - Blue
	// I - Intensity (in the case of gray)
	// P - Palette
	COLOR_FORMAT_ARGB8888,
	COLOR_FORMAT_ABGR8888,
	COLOR_FORMAT_RGB888,
	COLOR_FORMAT_BGR888,
	COLOR_FORMAT_ARGB1555,
	COLOR_FORMAT_ABGR1555,
	COLOR_FORMAT_RGB565,
	COLOR_FORMAT_BGR565,
	COLOR_FORMAT_I8,
	COLOR_FORMAT_P8,
	COLOR_FORMAT_I4,
	COLOR_FORMAT_P4,
	COLOR_FORMAT_I1,
};

typedef struct
{
	uint32_t Width;
	uint32_t Height;
	uint32_t Pitch;
	short    Bpp;
	short    ColorFormat;
}
IOCTL_FRAMEBUFFER_INFO, *PIOCTL_FRAMEBUFFER_INFO;

typedef struct
{
	uint32_t Left;
	uint32_t Top;
	uint32_t Width;
	uint32_t Height;
}
IOCTL_FRAMEBUFFER_FLUSH_INFO, *PIOCTL_FRAMEBUFFER_FLUSH_INFO;

//
// Keyboard devices expose a stream of bytes.  The stream of bytes is only made of a
// single 2-byte key code, where 15 bits represent the key code, and the MSB represents
// the released state of the key.
//

enum
{
	// Based off of the standard 104-key keyboard.  Add more keys if necessary!
	KBDEV_KEY_ESCAPE,
	KBDEV_KEY_F1,
	KBDEV_KEY_F2,
	KBDEV_KEY_F3,
	KBDEV_KEY_F4,
	KBDEV_KEY_F5,
	KBDEV_KEY_F6,
	KBDEV_KEY_F7,
	KBDEV_KEY_F8,
	KBDEV_KEY_F9,
	KBDEV_KEY_F10,
	KBDEV_KEY_F11,
	KBDEV_KEY_F12,
	KBDEV_KEY_PRINT_SCREEN,
	KBDEV_KEY_SCROLL_LOCK,
	KBDEV_KEY_PAUSE_BREAK,
	KBDEV_KEY_OEM_TILDE,
	KBDEV_KEY_MINUS,
	KBDEV_KEY_EQUALS,
	KBDEV_KEY_BACKSPACE,
	KBDEV_KEY_NUM_LOCK,
	KBDEV_KEY_NUMBER_PAD_DIVIDE,
	KBDEV_KEY_NUMBER_PAD_MULTIPLY,
	KBDEV_KEY_NUMBER_PAD_MINUS,
	KBDEV_KEY_NUMBER_PAD_PLUS,
	KBDEV_KEY_NUMBER_PAD_POINT,
	KBDEV_KEY_NUMBER_PAD_ENTER,
	KBDEV_KEY_TAB,
	KBDEV_KEY_OEM_LEFT_BRACKET,
	KBDEV_KEY_OEM_RIGHT_BRACKET,
	KBDEV_KEY_OEM_BACKSLASH,
	KBDEV_KEY_CAPS_LOCK,
	KBDEV_KEY_OEM_SEMICOLON,
	KBDEV_KEY_OEM_APOSTROPHE,
	KBDEV_KEY_ENTER,
	KBDEV_KEY_OEM_COMMA,
	KBDEV_KEY_OEM_PERIOD,
	KBDEV_KEY_OEM_SLASH,
	KBDEV_KEY_SPACE,
	KBDEV_KEY_LEFT_SHIFT,
	KBDEV_KEY_LEFT_CONTROL,
	KBDEV_KEY_LEFT_ALT,
	KBDEV_KEY_LEFT_SUPER,
	KBDEV_KEY_RIGHT_SHIFT,
	KBDEV_KEY_RIGHT_CONTROL,
	KBDEV_KEY_RIGHT_ALT,
	KBDEV_KEY_RIGHT_SUPER,
	KBDEV_KEY_MENU,
	KBDEV_KEY_LEFT_ARROW,
	KBDEV_KEY_UP_ARROW,
	KBDEV_KEY_RIGHT_ARROW,
	KBDEV_KEY_DOWN_ARROW,
	KBDEV_KEY_INSERT,
	KBDEV_KEY_DELETE,
	KBDEV_KEY_HOME,
	KBDEV_KEY_END,
	KBDEV_KEY_PAGE_UP,
	KBDEV_KEY_PAGE_DOWN,
	KBDEV_KEY_0,
	KBDEV_KEY_1,
	KBDEV_KEY_2,
	KBDEV_KEY_3,
	KBDEV_KEY_4,
	KBDEV_KEY_5,
	KBDEV_KEY_6,
	KBDEV_KEY_7,
	KBDEV_KEY_8,
	KBDEV_KEY_9,
	KBDEV_KEY_A,
	KBDEV_KEY_B,
	KBDEV_KEY_C,
	KBDEV_KEY_D,
	KBDEV_KEY_E,
	KBDEV_KEY_F,
	KBDEV_KEY_G,
	KBDEV_KEY_H,
	KBDEV_KEY_I,
	KBDEV_KEY_J,
	KBDEV_KEY_K,
	KBDEV_KEY_L,
	KBDEV_KEY_M,
	KBDEV_KEY_N,
	KBDEV_KEY_O,
	KBDEV_KEY_P,
	KBDEV_KEY_Q,
	KBDEV_KEY_R,
	KBDEV_KEY_S,
	KBDEV_KEY_T,
	KBDEV_KEY_U,
	KBDEV_KEY_V,
	KBDEV_KEY_W,
	KBDEV_KEY_X,
	KBDEV_KEY_Y,
	KBDEV_KEY_Z,
	KBDEV_KEY_NUMBER_PAD_0,
	KBDEV_KEY_NUMBER_PAD_1,
	KBDEV_KEY_NUMBER_PAD_2,
	KBDEV_KEY_NUMBER_PAD_3,
	KBDEV_KEY_NUMBER_PAD_4,
	KBDEV_KEY_NUMBER_PAD_5,
	KBDEV_KEY_NUMBER_PAD_6,
	KBDEV_KEY_NUMBER_PAD_7,
	KBDEV_KEY_NUMBER_PAD_8,
	KBDEV_KEY_NUMBER_PAD_9,

	// Flags OR-ed onto the key code.
	KBDEV_KEY_RELEASE = 0x8000,

	KBDEV_KEY_NONE = 0xFFFF,
};

//
// Pointing devices expose a stream of bytes.  Unlike keyboard devices, the stream of bytes
// has a more complex structure and is split up into packets.
//
// This is the structure that defines a pointing device frame. DON'T CHANGE ITS SIZE!
// (If you want to support a new packet format, use an I/O control code.)
//
// PointerId - The # of the associated pointer.  Note that the pointer device may report
//             multiple different pointer IDs even if only one pointer is active at a time.
//             However, while the user has activated a pointer, the pointer ID of that
//             pointer will not change. (e.g. if a device starts touching with finger #1, the
//             touch won't suddenly be turned into finger #2 until after they release it.)
//
// Flags - List of flags that pertain to this pointer ID.
//
// X, Y - Either the delta movement, or the absolute position, of the pointer. In absolute
//        mode, the X and Y coordinates are represented in 0.15 fixed point format (0-32767),
//        which can later be converted to the screen size.  In relative mode, delta movement
//        is reported according to the device's native units and must be converted.
//
typedef struct
{
	uint8_t PointerId;
	uint8_t Flags;
	int16_t X;
	int16_t Y;
}
IOCTL_POINTER_EVENT, *PIOCTL_POINTER_EVENT;

#define POINTER_RELEASE  0x01

#define POINTER_ABSOLUTE 0x02

//
// Beeper devices expose a stream of writes which can be written to, but not read.
//
// To use a beeper device, write the desired frequency as a 2-byte word.  If multiple
// writes have been performed within the same call, the space between writes is
// device-dependent.
//

//
// Video input devices expose a stream of bytes which can be used for synchronization.
// When a frame is returned by the video input device, a single byte write will be returned.
// (Even if multiple bytes are requested via OSReadFile, only one will be returned on output,
// and the value is to be ignored.)
//
// Most I/O is done through the specific I/O control codes and memory mapped I/O.
//
typedef struct
{
	uint32_t Width;
	uint32_t Height;
	uint32_t Pitch;
	short Bpp;
	short ColorFormat;
}
IOCTL_VIDEO_INPUT_INFO, *PIOCTL_VIDEO_INPUT_INFO;
