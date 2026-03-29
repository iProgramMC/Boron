/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	mm/mpw.h
	
Abstract:
	This header defines the interface with the Modified Page Writer (MPW).
	
Author:
	iProgramInCpp - 29 March 2026
***/
#pragma once

void MmModifiedPageWriterShutDown();

void MmStartFlushingModifiedPages();
