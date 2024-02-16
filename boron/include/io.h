/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io.h
	
Abstract:
	This header file contains the definitions related to
	the Boron kernel I/O manager.
	
Author:
	iProgramInCpp - 15 February 2024
***/
#pragma once

#include <main.h>
#include <ob.h>

#include <io/devobj.h>
#include <io/drvobj.h>
#include <io/fileobj.h>
#include <io/irp.h>

bool IoInitSystem();

