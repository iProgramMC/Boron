/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke.h
	
Abstract:
	This header file contains the amalgam include file
	for the kernel core.
	
Author:
	iProgramInCpp - 20 August 2023
***/
#pragma once

#include <main.h>
#include <limreq.h>
#include <arch.h>

#include <kes.h>
#include <atom.h>

#include <ke/mode.h>
#include <ke/ipl.h>
#include <ke/locks.h>
#include <ke/prcb.h>
#include <ke/stats.h>
#include <ke/irq.h>
#include <ke/int.h>
#include <ke/dbg.h>
#include <ke/ver.h>
#include <ke/smp.h>
#include <ke/crash.h>
