/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	device.c

Abstract:
	This module implements the device supporting routines
	for the iPod/iPhone button driver.

Author:
	iProgramInCpp - 24 October 2026
***/
#include "ipodbtns.h"

#define KEYBOARD_PRIORITY_BOOST 12

static PDEVICE_OBJECT BtnsDeviceObject;

static KEVENT BtnsAvailableEvent;

typedef struct FCB_EXTENSION
{
	uint8_t KeyCodes[256];
	uint8_t HighByteLatch;
	bool ReadingHighByte;
	int Tail, Head;
}
FCB_EXTENSION, *PFCB_EXTENSION;

#define Extension(Fcb) ((PFCB_EXTENSION) Fcb->Extension)

static PFCB_EXTENSION BtnsFcbExtension;

static KSPIN_LOCK BtnsSpinLock;

void BtnsAddKeyToBuffer(uint8_t Code)
{
	PFCB_EXTENSION Ext = BtnsFcbExtension;
	if (!Ext)
		return;

	KIPL Ipl;
	KeAcquireSpinLock(&BtnsSpinLock, &Ipl);

	int NewHead = (Ext->Head + 1) % sizeof (Ext->KeyCodes);
	if (NewHead == Ext->Tail) {
		KeReleaseSpinLock(&BtnsSpinLock, Ipl);
		DbgPrint("Key press dropped due to full buffer");
		return;
	}

	Ext->KeyCodes[Ext->Head] = Code;
	Ext->Head = NewHead;
	KeReleaseSpinLock(&BtnsSpinLock, Ipl);
	KeSetEvent(&BtnsAvailableEvent, KEYBOARD_PRIORITY_BOOST);
}

uint8_t BtnsReadKeyFromBuffer()
{
	PFCB_EXTENSION Ext = BtnsFcbExtension;
	if (!Ext)
		return 0xFF;

	KIPL Ipl;
	KeAcquireSpinLock(&BtnsSpinLock, &Ipl);

	if (Ext->Head == Ext->Tail) {
		KeReleaseSpinLock(&BtnsSpinLock, Ipl);
		return 0xFF;
	}

	int NewTail = (Ext->Tail + 1) % sizeof (Ext->KeyCodes);
	uint8_t Read = Ext->KeyCodes[Ext->Tail];
	Ext->Tail = NewTail;

	KeReleaseSpinLock(&BtnsSpinLock, Ipl);
	return Read;
}

uint16_t BtnsTranslateToKbdevCode(uint8_t Code)
{
	bool Released = Code & BTN_RELEASED;
	Code &= ~BTN_RELEASED;

	uint16_t KbdevCode = 0;

	switch (Code)
	{
		case BTN_HOLD:     KbdevCode = KBDEV_KEY_P; break;
		case BTN_HOME:     KbdevCode = KBDEV_KEY_H; break;
		case BTN_RINGER:   KbdevCode = KBDEV_KEY_R; break;
		case BTN_VOL_UP:   KbdevCode = KBDEV_KEY_UP_ARROW;   break;
		case BTN_VOL_DOWN: KbdevCode = KBDEV_KEY_DOWN_ARROW; break;
	}

	if (Released)
		KbdevCode |= KBDEV_KEY_RELEASE;

	return KbdevCode;
}

BSTATUS BtnsRead(
	PIO_STATUS_BLOCK Iosb,
	UNUSED PFCB Fcb,
	UNUSED uint64_t Offset,
	PMDL Mdl,
	uint32_t Flags
)
{
	ASSERT(Extension(Fcb) == BtnsFcbExtension);

	void* Buffer = NULL;
	BSTATUS Status;
	Status = MmMapPinnedPagesMdl(Mdl, &Buffer);
	if (FAILED(Status))
	{
		Iosb->Status = Status;
		return Status;
	}

	size_t Length = Mdl->ByteCount;

	uint8_t* BufferBytes = Buffer;

	for (size_t i = 0; i < Length; )
	{
		if (BtnsFcbExtension->ReadingHighByte)
		{
			BufferBytes[i] = BtnsFcbExtension->HighByteLatch;
			BtnsFcbExtension->ReadingHighByte = false;
			i++;
			continue;
		}
		else
		{
			uint8_t RawCode = BtnsReadKeyFromBuffer();
			uint16_t Code = BtnsTranslateToKbdevCode(RawCode);
			if (RawCode != 0xFF && Code != KBDEV_KEY_NONE)
			{
				BufferBytes[i] = Code & 0xFF;
				BtnsFcbExtension->ReadingHighByte = true;
				BtnsFcbExtension->HighByteLatch = (uint8_t)(Code >> 8);
				i++;
				continue;
			}
		}

		// Have to block or return.
		if ((Flags & IO_RW_NONBLOCK) || ((Flags & IO_RW_NONBLOCK_UNLESS_EMPTY) && i != 0))
		{
			// Can't block, so return now.
			Iosb->Status = STATUS_BLOCKING_OPERATION;
			Iosb->BytesRead = i;
			return Iosb->Status;
		}

		BSTATUS Status = KeWaitForSingleObject(&BtnsAvailableEvent, true, TIMEOUT_INFINITE, MODE_KERNEL);

		if (Status != STATUS_SUCCESS)
		{
			Iosb->BytesRead = i;
			Iosb->Status = Status;
			return Status;
		}

		// The read will be re-tried.
	}

	Iosb->BytesRead = Length;
	Iosb->Status = STATUS_SUCCESS;

	//MmUnmapPagesMdl(Mdl);

	return STATUS_SUCCESS;
}

bool BtnsSeekable(UNUSED PFCB Fcb)
{
	return false;
}

IO_DISPATCH_TABLE BtnsDispatchTable = {
	.Read = &BtnsRead,
	.Seekable = &BtnsSeekable,
	.Flags = DISPATCH_FLAG_EXCLUSIVE,
};

extern PDRIVER_OBJECT gDriverObject;

BSTATUS BtnsCreateDeviceObject()
{
	KeInitializeEvent(
		&BtnsAvailableEvent,
		EVENT_SYNCHRONIZATION,
		false
	);

	KeInitializeSpinLock(&BtnsSpinLock);

	BSTATUS Status = IoCreateDevice(
		gDriverObject,
		0,                      // DeviceExtensionSize
		sizeof (FCB_EXTENSION), // FcbExtensionSize
		"I8042PrtKeyboard",
		DEVICE_TYPE_CHARACTER,
		false,
		&BtnsDispatchTable,
		&BtnsDeviceObject
	);

	if (FAILED(Status))
	{
		DbgPrint("IoCreateDevice for iPod Buttons failed: %d", Status);
		return Status;
	}

	BtnsFcbExtension = Extension(BtnsDeviceObject->Fcb);

	return Status;
}
