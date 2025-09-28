/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/ver.h
	
Abstract:
	This header file contains the definitions
	related to the versioning of the operating system.
	
Author:
	iProgramInCpp - 28 October 2023
***/
#pragma once

#include <main.h>

#define VER_MAJOR(vn) ((vn) >> 24)
#define VER_MINOR(vn) (((vn) >> 16) & 0xFF)
#define VER_BUILD(vn) (vn & 0xFFFF)

#define VER(maj, min, pat) ((((maj) & 0xFF) << 24) | (((min) & 0xFF) << 16) | ((pat) & 0xFFFF))

int KeGetVersionNumber();
