/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/fileobj.h
	
Abstract:
	
	
Author:
	iProgramInCpp - 22 June 2024
***/
#pragma once

#include "fcb.h"

typedef struct _FILE_OBJECT
{
	PFCB Fcb;
}
FILE_OBJECT, *PFILE_OBJECT;
