/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	inttst.c
	
Abstract:
	This module implements the interrupt dispatch test for
	the test driver.
	
Author:
	iProgramInCpp - 1 April 2024
***/
#include "utils.h"
#include <ke.h>

static KSPIN_LOCK Int1SyncLock;
static KSPIN_LOCK Int2SyncLock;
static KINTERRUPT Int1;
static KINTERRUPT Int2;
static bool Int1Fired, Int2Fired;

void Int1Routine(PKINTERRUPT Int, void* Ctx)
{
	DbgPrint("Int1Routine (%p, %p)!", Int, Ctx);
	Int1Fired = true;
}

void Int2Routine(PKINTERRUPT Int, void* Ctx)
{
	DbgPrint("Int2Routine (%p, %p)!", Int, Ctx);
	Int2Fired = true;
}

void PerformIntTest()
{
	// Test 1.
	Int1Fired = Int2Fired = false;
	
	KeInitializeInterrupt(&Int1, Int1Routine, NULL, &Int1SyncLock, 0x80, 0x8, true);
	KeInitializeInterrupt(&Int2, Int2Routine, NULL, &Int2SyncLock, 0x80, 0x8, true);
	
	if (!KeConnectInterrupt(&Int1)) KeCrash("Test fail: cannot connect Int1 (first test)");
	if (!KeConnectInterrupt(&Int2)) KeCrash("Test fail: cannot connect Int2 (first test)");
	
	ASM("int $0x80":::"memory");
	
	if (!Int1Fired) KeCrash("Test fail: int1 not fired (first test)");
	if (!Int2Fired) KeCrash("Test fail: int2 not fired (first test)");
	
	KeDisconnectInterrupt(&Int1);
	KeDisconnectInterrupt(&Int2);
	
	// Test 2.
	Int1Fired = Int2Fired = false;
	
	KeInitializeInterrupt(&Int1, Int1Routine, NULL, &Int1SyncLock, 0x80, 0x8, false);
	KeInitializeInterrupt(&Int2, Int2Routine, NULL, &Int2SyncLock, 0x80, 0x8, false);
	
	if (!KeConnectInterrupt(&Int1)) KeCrash("Test fail: cannot connect int1 (second test)");
	if ( KeConnectInterrupt(&Int2)) KeCrash("Test fail: can connect int2 (second test)");
	
	ASM("int $0x80":::"memory");
	
	if (!Int1Fired) KeCrash("Test fail: int1 not fired (second test)");
	if ( Int2Fired) KeCrash("Test fail: int2 fired (second test)");
	
	KeDisconnectInterrupt(&Int1);
}
