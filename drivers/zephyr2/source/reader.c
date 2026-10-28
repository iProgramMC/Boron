/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	reader.c

Abstract:
	This module implements the reader thread for the
	zephyr2 multitouch controller.

Licensing:
	Because this driver borrows significantly from OpeniBoot, this driver's
	source code is licensed separately under the GNU General Public License
	version 3.0.  You should have received a copy of the GNU General Public
	License along with this source code file, which is available at the
	`drivers/LICENSE.ARM` path.  Otherwise, view the web page containing the
	license at https://gnu.org/licenses/gpl.html.

Author:
	iProgramInCpp - 27 October 2026
***/
#include "z2.h"

static uint8_t TxPacket[512];
static uint8_t RxPacket[512];

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
	uint8_t Tx[16], Rx[16];
	memset(Tx, 0, sizeof Tx);

	if (MtFlipNop)
		Tx[0] = 0xeb; // frame read alt
	else
		Tx[0] = 0xea; // frame read

	Tx[1] = MtCurNop;
	Tx[2] = 0; // don't read yet

	uint16_t Checksum = MtChecksum(Tx, 0, 14);
	Tx[14] = Checksum & 0xFF;
	Tx[15] = (Checksum >> 8) & 0xFF;

	MtSpiTxRx(&MtNormalSpeed, Tx, sizeof Tx, Rx, sizeof Rx);

	Checksum = MtChecksum(Rx, 0, 14);
	uint16_t RecvChecksum = Rx[14] | (Rx[15] << 8);

	if (RecvChecksum != Checksum)
	{
		// not sure why OpeniBoot does a udelay here
		return false;
	}

	*OutLength = Rx[1] | (Rx[2] << 8);
	return true;
}

void MtOnFinishProcessingFrame()
{
	if (MtFlipNop)
	{
		if (MtCurNop == 1)
			MtCurNop = 2;
		else
			MtCurNop = 1;
	}
}

void MtProcessFrame(uint8_t* Frame, size_t Length)
{
	MT_FRAME_HEADER FrameHeader;
	MT_FINGER_DATA FingerData;

	if (Length < sizeof FrameHeader)
	{
		DbgPrint("zephyr2: expected %zu bytes but got %zu bytes", sizeof FrameHeader, Length);
		return;
	}

	memcpy(&FrameHeader, Frame, sizeof FrameHeader);

	if (FrameHeader.Type != 0x44 && FrameHeader.Type != 0x43)
	{
		LogMsg("zephyr2: unknown frame type 0x%02x", FrameHeader.Type);
		return;
	}

	if (FrameHeader.FingerCount == 0)
	{
		DbgPrint("zephyr2: No fingers");
		MtResetLastPosAllFingers();
		return;
	}

	DbgPrint("zephyr2: %d fingers, timestamp %u", FrameHeader.FingerCount, FrameHeader.Timestamp);

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

// This is called after the multitouch fires an ATN interrupt, inside the
// MtDpcRoutine handler.
void MtReadMultitouchFrame()
{
	size_t FrameLength = 0;
	if (!MtReadFrameLength(&FrameLength)) {
		DbgPrint("zephyr2: Cannot read frame length.");
		return;
	}

	if (FrameLength == 0)
	{
		MtOnFinishProcessingFrame();
		return;
	}

	if (FrameLength > sizeof TxPacket)
		FrameLength = sizeof TxPacket;

	FrameLength += 5;

	memset(TxPacket, 0, FrameLength);

	TxPacket[0] = MtFlipNop ? 0xeb : 0xea;
	TxPacket[1] = MtCurNop;
	TxPacket[2] = 1; // do read now

	uint16_t Checksum = MtChecksum(TxPacket, 0, 14);
	TxPacket[FrameLength - 2] = Checksum & 0xFF;
	TxPacket[FrameLength - 1] = (Checksum >> 8) & 0xFF;

	MtSpiTxRx(&MtNormalSpeed, TxPacket, FrameLength, RxPacket, FrameLength);

	if (RxPacket[0] != 0xea && !(RxPacket[0] != 0xeb && MtFlipNop))
	{
		DbgPrint("zephyr2: Frame header is wrong (received 0x%2x)", RxPacket[0]);
		return;
	}

	uint32_t Sum = 0;
	for (int i = 0; i < 5; i++)
		Sum += RxPacket[i];

	if ((Sum & 0xFF) != 0)
	{
		DbgPrint("zephyr2: LSB of first 5 bytes summed is not zero, it's 0x%02x", Sum & 0xFF);
		return;
	}

	uint32_t PacketLen = RxPacket[2] | (RxPacket[3] << 8);
	if (PacketLen <= 2)
	{
		DbgPrint("zephyr2: Got short packet");
		MtOnFinishProcessingFrame();
		return;
	}

	uint16_t CheckSum = MtChecksum(RxPacket, 5, 5 + PacketLen - 2);
	uint16_t RecvCheckSum = RxPacket[FrameLength - 2] | (RxPacket[FrameLength - 1] << 8);
	if (CheckSum != RecvCheckSum)
	{
		DbgPrint("zephyr2: bad checksum, received 0x%04x but got 0x%04x", RecvCheckSum, CheckSum);
		;//return;
	}

	MtProcessFrame(RxPacket + 5, PacketLen - 2);
	MtOnFinishProcessingFrame();
}

void MtClearScreen()
{
	// clear screen
	HalDisplayString("\x1b[2J\x1b[H");

	MtFillRectangle(0x00007F, 0, 0, MtFramebufferWidth(), MtFramebufferHeight());

	LogMsg("Boron Multitouch test");
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

	uint16_t Buffer;
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

		Result = MtCheckForEvents(true, MtEmulatorMode ? 60 : 33);
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
