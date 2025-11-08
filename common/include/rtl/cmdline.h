/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	rtl/cmdline.c
	
Abstract:
	This module implements the command line parser, as well as an environment
	variable related utility function.
	
Author:
	iProgramInCpp - 8 November 2025
***/
#pragma once

#include <stdint.h>
#include <stdbool.h>

size_t RtlEnvironmentLength(const char* Description);

BSTATUS RtlCommandLineStringToDescription(const char* String, char** Description);
