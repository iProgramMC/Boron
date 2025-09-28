/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ps/services.h
	
Abstract:
	This header file defines most of the Ps-offered
	system service functions.
	
Author:
	iProgramInCpp - 22 April 2025
***/
#pragma once

NO_RETURN void OSExitThread();

NO_RETURN void OSExitProcess(int ExitCode);

BSTATUS OSSetExitCode(int ExitCode);

BSTATUS OSGetExitCodeProcess(HANDLE ProcessHandle, int* ExitCodeOut);
