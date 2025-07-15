/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	kes.h
	
Abstract:
	This header file contains the publicly exposed structure
	definitions for Boron's Kernel Core Subsystem.
	
Author:
	iProgramInCpp - 22 April 2025
***/
#pragma once

// TODO: Define as uint32_t on 32-bit architectures.
typedef uint64_t KAFFINITY;

// Note. We define it as a qword, and cast it down later if needed.
#define AFFINITY_ALL ((KAFFINITY) ~0ULL)

typedef int KPRIORITY;

enum
{
	EVENT_SYNCHRONIZATION,
	EVENT_NOTIFICATION,
};

typedef NO_RETURN void(*PKTHREAD_START)(void* Context);

