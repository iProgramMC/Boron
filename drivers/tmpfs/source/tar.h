/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	tar.h
	
Abstract:
	This header file defines structs and function prototypes
	pertaining to the TAR file format.
	
Author:
	iProgramInCpp - 3 November 2025
***/
#pragma once

#include <stdint.h>

typedef struct 
{
	union
	{
		struct
		{
			char Name[100];
			char Mode[8];
			char Uid[8];
			char Gid[8];
			char Size[12];
			char ModifyTime[12];
			char Check[8];
			char Type;
			char LinkName[100];
			char Ustar[8];
			char Owner[32];
			char Group[32];
			char Major[8];
			char Minor[8];
			char Prefix[155];
		};
		
		char Block[512];
	};
	
	char Buffer[];
}
TAR_UNIT, *PTAR_UNIT;
