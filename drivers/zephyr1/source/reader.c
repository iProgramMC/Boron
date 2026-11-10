/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	reader.c

Abstract:
	This module implements the reader thread for the
	zephyr1 multitouch controller.

Licensing:
	Because this driver borrows significantly from OpeniBoot, this driver's
	source code is licensed separately under the GNU General Public License
	version 3.0.  You should have received a copy of the GNU General Public
	License along with this source code file, which is available at the
	`drivers/LICENSE.ARM` path.  Otherwise, view the web page containing the
	license at https://gnu.org/licenses/gpl.html.

Author:
	iProgramInCpp - 10 November 2026
***/
#include "z1.h"

static uint8_t TxPacket[1024];
static uint8_t RxPacket[1024];

#define MAX_FINGERS 16 // way more than the maximum
static int LastX[MAX_FINGERS], LastY[MAX_FINGERS];

static bool DrawLine = true;

const uint32_t Colors[MAX_FINGERS] =
{
	0xFF0000,  // RED
	0x00FF00,  // GREEN
	0xFF0000,  // YELLOW
	0xFF00FF,  // MAGENTA
	0x00FFFF,  // CYAN
	0xFFFFFF,  // WHITE
	0x000000,  // BLACK
	// padding...
	0xFFFFFF,  // #7
	0xFFFFFF,  // #8
	0xFFFFFF,  // #9
	0xFFFFFF,  // #10
	0xFFFFFF,  // #11
	0xFFFFFF,  // #12
	0xFFFFFF,  // #13
	0xFFFFFF,  // #14
	0xFFFFFF,  // #15
};

void MtResetLastPosFinger(int FingerId)
{
	ASSERT(FingerId >= 0 && FingerId < MAX_FINGERS);
	LastX[FingerId] = -1;
	LastY[FingerId] = -1;
}

void MtResetLastPosAllFingers()
{
	for (int i = 0; i < MAX_FINGERS; i++) {
		MtResetLastPosFinger(i);
	}
}

bool MtReadFrameLength(size_t* OutLength)
{
	uint8_t Tx[8], Rx[8];
	memset(Tx, MtCurNop, sizeof Tx);

	for (int Try = 0; Try < 4; Try++)
	{
		MtSpiTxRx(&MtReaderSpeed, Tx, sizeof Tx, Rx, sizeof Rx);
		if (Rx[0] != 0xAA)
		{
			// not sure why OpeniBoot does a udelay here
			//OSSleep(1);
			continue;
		}

		int TLen = (Rx[4] << 8) | Rx[5];
		int TLenCheckSum = (Rx[4] + Rx[5]) & 0xFFFF;
		int CheckSum = (Rx[6] << 8) | Rx[7];

		if (TLenCheckSum != CheckSum)
		{
			// ... or here
			continue;
		}

		if (TLen == 0) {
			// Declare it as a failure for now...
			// That seems to do the trick.
			continue;
		}

		*OutLength = TLen;
		return true;
	}

	return false;
}

void MtOnFinishProcessingFrame()
{
	if (MtCurNop == 0x64)
		MtCurNop = 0x65;
	else
		MtCurNop = 0x64;
}

void MtProcessFrame(uint8_t* Frame, size_t Length)
{
	MT_FRAME_HEADER FrameHeader;
	MT_FINGER_DATA FingerData;

	if (Length < sizeof FrameHeader)
	{
		DbgPrint("zephyr1: expected %zu bytes but got %zu bytes", sizeof FrameHeader, Length);
		return;
	}

	memcpy(&FrameHeader, Frame, sizeof FrameHeader);

	if (FrameHeader.Type != 0x44 && FrameHeader.Type != 0x43)
	{
		DbgPrint("zephyr1: unknown frame type 0x%02x", FrameHeader.Type);
		return;
	}

	if (FrameHeader.FingerCount == 0)
	{
		DbgPrint("zephyr1: No fingers");
		MtResetLastPosAllFingers();
		return;
	}

	DbgPrint("zephyr1: %d fingers, timestamp %u", FrameHeader.FingerCount, FrameHeader.Timestamp);

	uint8_t* FingerDataSrc = Frame + FrameHeader.HeaderLength;
	for (int i = 0; i < FrameHeader.FingerCount; i++)
	{
		memcpy(&FingerData, FingerDataSrc, sizeof FingerData);
		FingerDataSrc += FrameHeader.FingerDataLength;

		DbgPrint(
			"	finger #%d: id:%d event:%d x:%d y:%d vx:%d vy:%d r2:%d r3:%d a:%d r1:%d cd:%d",
			FingerData.Id,
			FingerData.Event,
			FingerData.X,
			FingerData.Y,
			FingerData.VelX,
			FingerData.VelY,
			FingerData.Radius2,
			FingerData.Radius3,
			FingerData.Angle,
			FingerData.Radius1,
			FingerData.ContactDensity
		);

		ASSERT(MtSensorWidth != 0 && MtSensorHeight != 0);
		int ActualX = FingerData.X * MtFramebufferWidth() / MtSensorWidth;
		int ActualY = (MtSensorHeight - FingerData.Y) * MtFramebufferHeight() / MtSensorHeight;

		int Id = FingerData.Id;
		if (DrawLine) {
			if (LastX[Id] == -1 || LastY[Id] == -1)
				MtDrawLine(Colors[Id], ActualX, ActualY, ActualX, ActualY);
			else
				MtDrawLine(Colors[Id], LastX[Id], LastY[Id], ActualX, ActualY);
		} else {
			MtDrawLine(Colors[Id], ActualX, ActualY, ActualX, ActualY);
		}

		LastX[Id] = ActualX;
		LastY[Id] = ActualY;
	}
}

bool MtReadResultData(size_t Length)
{
	memset(TxPacket, 0x68, sizeof TxPacket);

	for (int Try = 0; Try < 4; Try++)
	{
		MtSpiTxRx(&MtReaderSpeed, TxPacket, Length, RxPacket, Length);

		if (RxPacket[0] != 0xAA) {
			continue;
		}

		int ReceivedCheckSum = (RxPacket[Length - 2] << 8) | RxPacket[Length - 1];
		int CheckSum = 0;
		for (size_t i = 1; i < Length - 2; i++) {
			CheckSum += RxPacket[i];
		}
		CheckSum &= 0xFFFF;

		if (CheckSum != ReceivedCheckSum) {
			continue;
		}

		return true;
	}

	return false;
}

// This is called after the multitouch fires an ATN interrupt, inside the
// MtDpcRoutine handler.
void MtReadMultitouchFrame()
{
	size_t FrameLength = 0;
	if (!MtReadFrameLength(&FrameLength)) {
		DbgPrint("zephyr1: Cannot read frame length.");
		return;
	}

	if (FrameLength == 0)
	{
		MtOnFinishProcessingFrame();
		return;
	}

	if (FrameLength > sizeof TxPacket)
		FrameLength = sizeof TxPacket;

	FrameLength += 1;

	if (!MtReadResultData(FrameLength)) {
		DbgPrint("zephyr1: Cannot read result data.");
		return;
	}

	MtProcessFrame(RxPacket + 1, FrameLength - 3);
	MtOnFinishProcessingFrame();
}

void MtClearScreen()
{
	// clear screen
	// ooo, red this time
	HalDisplayString("\x1b[2J\x1b[H\x1b[41m");

	MtFillRectangle(0x7F0000, 0, 0, MtFramebufferWidth(), MtFramebufferHeight());

	LogMsg("Boron Multitouch test (Z1 mode)");
	LogMsg("Press HOME to erase screen");
	LogMsg("Press POWER to draw %s and clear", DrawLine?"dots":"lines");
}

HANDLE MtKeypadHandle;
IO_STATUS_BLOCK MtIosb;

void MtOpenKeypad()
{
	OBJECT_ATTRIBUTES Attributes;
	Attributes.RootDirectory = HANDLE_NONE;
	// there is no actual keyboard, it's just what I called ipodbtns for now
	Attributes.ObjectName = "/Devices/I8042PrtKeyboard";
	Attributes.ObjectNameLength = strlen(Attributes.ObjectName);
	Attributes.OpenFlags = 0;

	BSTATUS Status = OSCreateFile(
		&MtKeypadHandle,
		&Attributes,
		&MtIosb,
		0, // FileAttributes
		0, // AllocationSize
		IO_OPEN,
		IO_FILE_ANY_TYPE,
		NULL, // TargetName
		0     // TargetNameLength
	);

	if (FAILED(Status))
	{
		LogMsg("Failed to open %s: %s", Attributes.ObjectName, RtlGetStatusString(Status));
		MtKeypadHandle = HANDLE_NONE;
	}
}

void MtListenForClearCommand()
{
	if (MtKeypadHandle == HANDLE_NONE)
		return;

	uint16_t Buffer = 0;
	BSTATUS Status = OSReadFile(
		&MtIosb,
		MtKeypadHandle,
		0, // ByteOffset
		&Buffer,
		sizeof Buffer,
		IO_RW_NONBLOCK
	);

	if (IOFAILED(Status)) {
		LogMsg("Failed to read from keypad: %s", RtlGetStatusString(Status));
		return;
	}

	if (MtIosb.BytesRead != sizeof(uint16_t)) {
		return;
	}

	// Home button is pressed?
	if (Buffer == KBDEV_KEY_ENTER) {
		MtClearScreen();
	}
	if (Buffer == KBDEV_KEY_P) {
		DrawLine ^= 1;
		MtClearScreen();
	}
}

void MtRunMultitouch()
{
	MtInitializeTestDisplay();

	MtClearScreen();

	MtOpenKeypad();

	while (true)
	{
		UNUSED bool Result;

		MtListenForClearCommand();

		Result = MtCheckForEvents(true, 33);
		if (!Result)
		{
			// Didn't receive touch data in more than 33 ms, that means they probably
			// moved the finger away from the device
			MtResetLastPosAllFingers();
			continue;
		}

		MtReadMultitouchFrame();
	}
}
