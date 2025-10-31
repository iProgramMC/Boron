/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ex/bootcfg.h
	
Abstract:
	This header file defines the executive's boot command
	line configuration interface.
	
Author:
	iProgramInCpp - 31 October 2025
***/
#pragma once

#define CONFIG_YES "yes"
#define CONFIG_NO "no"

// Retrieves a config value, and if it wasn't found, returns ValueIfNotFound.
const char* ExGetConfigValue(const char* Key, const char* ValueIfNotFound);

#define ExIsConfigValue(Key, Value) (strcmp(ExGetConfigValue((Key), ""), (Value)) == 0)
