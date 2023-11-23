/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/affinity.h
	
Abstract:
	This header file defines the affinity type.
	
Author:
	iProgramInCpp - 23 November 2023
***/
#pragma once

// TODO: Define as uint32_t on 32-bit architectures.
typedef uint64_t KAFFINITY;

// Note. We define it as a qword, and cast it down later if needed.
#define AFFINITY_ALL ((KAFFINITY) ~0ULL)
