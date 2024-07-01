/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	balltst.c
	
Abstract:
	This module implements the ball test for the test driver.
	
Author:
	iProgramInCpp - 19 November 2023
***/
#include "utils.h"
#include <hal.h>
#include <string.h>

#define P(n) ((void*) (uintptr_t) (n))

#define THREADCOUNT 64

// Updates for each processor ID.
int Updates[64];

NO_RETURN void TestThread1()
{
	// Display a simple run timer
	for (int i = 0; ; i++)
	{
		LogMsg("\x1B[2;64H     =====   %02d:%02d   =====     ", i/60, i%60);
		PerformDelay(999, NULL);
	}
}

NO_RETURN void TestThread2()
{
	for (int i = 0; i < 20; i++)
	{
		LogMsg("\x1B[1;1HMy second thread's still running!!  %d                         ", i);
		PerformDelay(100, NULL);
	}
	
	LogMsg("\x1B[1;40H\x1B[33mDone.\x1B[0m");
	KeTerminateThread(0);
}

void BallTestDpc(UNUSED PKDPC Dpc, UNUSED void* Context, UNUSED void* SysAux1, UNUSED void* SysAux2)
{
	//DbgPrint("Hello from DPC %p, %p", Dpc, Context);
}

const int LimitLeft   = 4;
const int LimitTop    = 4;
const int LimitRight  = 120;
const int LimitBottom = 40;

void DisplayBorder()
{
	char Buffer[64];
#define PRINTF(...) do { \
	snprintf(Buffer, sizeof(Buffer), __VA_ARGS__);   \
	HalDisplayString(Buffer);                        \
} while (0)
	
	PRINTF("\x1B[%d;%dH+", LimitTop,        LimitLeft);
	PRINTF("\x1B[%d;%dH+", LimitTop,        LimitRight + 1);
	PRINTF("\x1B[%d;%dH+", LimitBottom + 1, LimitLeft);
	PRINTF("\x1B[%d;%dH+", LimitBottom + 1, LimitRight + 1);
	
	PRINTF("\x1B[%d;%dH", LimitTop, LimitLeft + 1);
	for (int i = LimitLeft + 1; i <= LimitRight; i++)
		HalDisplayString("=");
	PRINTF("\x1B[%d;%dH", LimitBottom + 1, LimitLeft + 1);
	for (int i = LimitLeft + 1; i <= LimitRight; i++)
		HalDisplayString("=");
	
	for (int i = LimitTop + 1; i <= LimitBottom; i++)
	{
		PRINTF("\x1B[%d;%dH|", i, LimitLeft);
		PRINTF("\x1B[%d;%dH|", i, LimitRight + 1);
	}
}

int RNG()
{
	static int Seed = 0;
	Seed = HalGetTickCount();
	
	int N1 = AtAddFetch(Seed, 712124624);
	int N2 = AtAddFetch(Seed, 894678516);
	int N3 = AtFetchAdd(Seed, 1754689821);
	
	return (N1 + N2 + N3) & 0x7FFFFFFF;
}

int RNGRange(int Min, int Max)
{
	return RNG() % (Max - Min) + Min;
}

NO_RETURN void BallTest()
{
	KDPC DrvDpc;
	KeInitializeDpc(&DrvDpc, BallTestDpc, KeGetCurrentThread());
	
	int LimitRightTF = LimitLeft + (LimitRight - LimitLeft) / 2;
	int BallX = RNGRange(LimitLeft, LimitRightTF - 1);
	int BallY = RNGRange(LimitTop, LimitBottom - 1);
	
	int VelX = RNGRange(0, 2) ? 1 : -1;
	int VelY = RNGRange(0, 2) ? 1 : -1;
	int Color = RNGRange(0, 8);
	
	int BallOldX = BallX;
	int BallOldY = BallY;
	int TickCounter = 0;
	
	// Perform initial delay before balls show up
	PerformDelay(2000 + RNGRange(0, 1000), &DrvDpc);
	
	bool FirstTick = true;
	
	while (true)
	{
		int OldBallX = BallX, OldBallY = BallY;
		
		BallX += VelX;
		BallY += VelY;
		
		bool Revert = false;
		
		// Bounce
		if (BallX < LimitLeft)
			Revert = true, VelX = -VelX;
		if (BallY < LimitTop)
			Revert = true, VelY = -VelY;
		if (BallX >= LimitRightTF)
			Revert = true, VelX = -VelX;
		if (BallY >= LimitBottom)
			Revert = true, VelY = -VelY;
		
		// Revert the position so the ball doesn't appear to slide
		if (Revert)
		{
			BallX = OldBallX;
			BallY = OldBallY;
		}
		
		int BallOldXTF   = LimitLeft + (BallOldX - LimitLeft) * 2;
		int BallXTF      = LimitLeft + (BallX    - LimitLeft) * 2;
		
		char Buffer[64];
		snprintf(Buffer, sizeof Buffer, "\x1B[%d;%dH\x1B[%dm  \x1B[0m\x1B[%d;%dH  ", BallY + 1, BallXTF + 1, 100 + Color, BallOldY + 1, BallOldXTF + 1);
		HalDisplayString(Buffer);
		
		if (!Revert)
		{
			BallOldX = BallX;
			BallOldY = BallY;
		}
		
		if (FirstTick)
			PerformDelay(1000, &DrvDpc);
		
		int CurrentPID = KeGetCurrentPRCB()->Id;
		snprintf(Buffer, sizeof Buffer, "\x1B[2;%dH\x1B[92m%d\x1B[0m", 20 + 12 * CurrentPID, ++Updates[CurrentPID]);
		HalDisplayString(Buffer);
		
		FirstTick = false;
		
		//PerformDelay(200 + RNGRange(0, 400), &DrvDpc);
		PerformDelay(16 + (TickCounter != 0), &DrvDpc);
		TickCounter++;
		if (TickCounter > 3)
			TickCounter = 0;
	}
}

static PKTHREAD Threads[THREADCOUNT];

static KWAIT_BLOCK WaitBlockStorage[THREADCOUNT];

void PerformBallTest()
{
	LogMsg(
		"\x1B[2J"     // Clear Screen
		"\x1B[H"      // Home
		"\x1B[0m"     // Reset Attributes
		"\x1B[?25l"   // Hide Cursor
	);
	
	DisplayBorder();
	
	LogMsg(
		"\x1B[H"      // Home
		"\x1B[0m"     // Reset Attributes
		"The Boron Operating System - BALL TEST\n"  // Print Info About The Test
		"%d Balls\n"  // Print Number Of Balls
		"Each ball is a separate thread that updates around 60 times per second.", // Print Info About What The Balls Are
		(int)ARRAY_COUNT(Threads));
	
	// Create thread for each ball
	for (int i = 0; i < (int)ARRAY_COUNT(Threads); i++)
	{
		Threads[i] = CreateThread(BallTest, P(i));
	}
	
	// Print info box in the middle
	int MiddleX = ((LimitRight - LimitLeft) - 50) / 2 + LimitLeft;
	int MiddleY = ((LimitBottom - LimitTop) - 3)  / 2 + LimitTop;
	
	LogMsg(
		"\x1B[7m"                                             // Invert The Text And Background Color
		"\x1B[%d;%dH"                                         // Go To (MiddleX, MiddleY)
		"+------------------------------------------------+"  // Print 50 Dashes And Two Pluses In The Corner
		"\x1B[%d;%dH"                                         // Go To (MiddleX, MiddleY + 1)
		"|      Watch the balls 'consume' this text!      |"  // Inform The User They're About To Watch The Balls 'Consume' This Text Box
		"\x1B[%d;%dH"                                         // Go To (MiddleX, MiddleY + 2)
		"+------------------------------------------------+"  // Print Another 50 Dashes And Two Pluses In The Corner
		"\x1B[m",                                             // Reset Attributes
		
		MiddleY + 1, MiddleX + 1,
		MiddleY + 2, MiddleX + 1,
		MiddleY + 3, MiddleX + 1
	);
	
	// Wait for the threads to quit (will never)
	// The WaitBlockArray is a required parameter for anything above THREAD_WAIT_OBJECTS.
	KeWaitForMultipleObjects(THREADCOUNT, (void**)Threads, WAIT_TYPE_ALL, false, TIMEOUT_INFINITE, WaitBlockStorage);
}

