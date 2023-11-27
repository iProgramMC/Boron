/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/mode.h
	
Abstract:
	This module defines the KPROCESSOR_MODE enum
	
Author:
	iProgramInCpp - 27 November 2023
***/
#pragma once

typedef enum KPROCESSOR_MODE_tag
{
	MODE_KERNEL,
	MODE_USER,
}
KPROCESSOR_MODE, *PKPROCESSOR_MODE;
