#include <main.h>
#include <ke.h>
#include <ex.h>
#include <string.h>
#include <hal.h>

#define THREADCOUNT 1

int* Test()
{
	static int stuff;
	return &stuff;
}

void KeYieldCurrentThread();

void PerformDelay(int Ms, PKDPC Dpc)
{
	KTIMER Timer;
	
	KeInitializeTimer(&Timer);
	KeSetTimer(&Timer, Ms, Dpc);
	
	int Status = KeWaitForSingleObject(&Timer.Header, false, Ms / 2);
	if (Status == STATUS_TIMEOUT)
	{
		DbgPrint("PerformDelay succeeded in waiting half the time :D");
		KeCancelTimer(&Timer);
	}
	else
	{
		DbgPrint("Womp, womp. Seems like it waited the entire time. Status is %d", Status);
	}
}

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
	KeTerminateThread();
}

void BallTestDpc(PKDPC Dpc, void* Context, UNUSED void* SysAux1, UNUSED void* SysAux2)
{
	DbgPrint("Hello from DPC %p, %p", Dpc, Context);
}

const int LimitLeft   = 2;
const int LimitTop    = 2;
const int LimitRight  = 124;
const int LimitBottom = 48;

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
	
	if (!Seed)
		Seed = HalGetTickCount();
	
	int N1 = AtAddFetch(Seed, 713545724);
	int N2 = AtAddFetch(Seed, 895878516);
	int N3 = AtFetchAdd(Seed, 1754624721);
	
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
	int Color = RNGRange(0, 8), Letter = RNGRange(0, 26);
	
	int BallOldX = BallX;
	int BallOldY = BallY;
	int TickCounter = 0;
	
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
			PerformDelay(2000, &DrvDpc);
		
		FirstTick = false;
		
		PerformDelay(16 + (TickCounter != 0), &DrvDpc);
		TickCounter++;
		if (TickCounter > 3)
			TickCounter = 0;
	}
}

PKTHREAD Threads[THREADCOUNT];

void CreateTestThread(int Index, PKTHREAD_START Routine)
{
	PKTHREAD Thread = Threads[Index] = KeCreateEmptyThread();
	
	KeInitializeThread(Thread, EX_NO_MEMORY_HANDLE, Routine, NULL);
	KeSetPriorityThread(Thread, PRIORITY_NORMAL);
	KeReadyThread(Thread);
}

int DriverEntry()
{
	LogMsg("\x1B[2J\x1B[H\x1B[0m\x1B[?25l");
	
	DisplayBorder();
	
	LogMsg("\x1B[H\x1B[0m%d Balls", (int)ARRAY_COUNT(Threads));
	
	for (int i = 0; i < (int)ARRAY_COUNT(Threads); i++)
		CreateTestThread(i, BallTest);
	
	*Test() = 69;
	
	return 42 + *Test();
}
