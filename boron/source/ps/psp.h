/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ps/psp.h
	
Abstract:
	This header defines private functions for Boron's
	process manager.
	
Author:
	iProgramInCpp - 30 August 2024
***/
#pragma once

#include <ke.h>
#include <ps.h>
#include <string.h>

bool PsCreateThreadType();

bool PsCreateProcessType();
