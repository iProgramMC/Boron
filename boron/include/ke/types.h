/***
	The Boron Operating System
	Copyright (C) 2023-2024 iProgramInCpp

Module name:
	ke/types.h
	
Abstract:
	This header file defines basic types used throughout ke.h.
	
Author:
	iProgramInCpp - 23 November 2023
***/
#pragma once

// TODO: Define as uint32_t on 32-bit architectures.
typedef uint64_t KAFFINITY;

// Note. We define it as a qword, and cast it down later if needed.
#define AFFINITY_ALL ((KAFFINITY) ~0ULL)

typedef int KPRIORITY;
