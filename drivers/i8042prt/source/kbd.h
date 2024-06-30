/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	main.c
	
Abstract:
	This header file defines extern functions and variables
	related to the i8042 keyboard driver.
	
Author:
	iProgramInCpp - 30 June 2024
***/
#pragma once

#include <io.h>
#include <string.h>
#include "i8042.h"

extern IO_DISPATCH_TABLE KbdDispatchTable;

extern BSTATUS KbdCreateDeviceObject();

