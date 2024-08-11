/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ex.h
	
Abstract:
	This header file defines prototypes for routines from the
	extended runtime library. The extended runtime library
	contains routines that depend on the kernel, unlike Rtl
	which is entirely freestanding.
	
Author:
	iProgramInCpp - 27 September 2023
***/
#ifndef BORON_EX_H
#define BORON_EX_H

#include <main.h>
#include <ex/handtab.h>
#include <ex/rwlock.h>
#include <ex/process.h>
#include <ex/object.h>

#endif//BORON_EX_H
