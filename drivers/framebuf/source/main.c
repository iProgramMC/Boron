/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	main.c
	
Abstract:
	This module implements the frame buffer driver.
	
Author:
	iProgramInCpp - 7 October 2025
***/
#include <ke.h>
#include <mm.h>
#include <io.h>
#include <string.h>

#define IOSB_STATUS(iosb, stat) (iosb->Status = stat)

PDRIVER_OBJECT FramebufferDriverObject;

typedef struct
{
	uintptr_t Address;
	uint32_t Width;
	uint32_t Height;
	uint32_t Pitch;
	uint32_t Bpp;
	size_t Size;
}
FCB_EXTENSION, *PFCB_EXTENSION;

#define EXT(Fcb) ((PFCB_EXTENSION)(Fcb->Extension))
#define FCB(Ext) CONTAINING_RECORD((Ext), FCB, Extension)
#define PREP_EXT PFCB_EXTENSION Ext = EXT(Fcb)

// TODO SECURITY: Verify that these do not have any exploits soon!  These are
// memory I/O ops running in kernel mode.  Maybe we don't even need these?

BSTATUS FramebufferRead(PIO_STATUS_BLOCK Iosb, UNUSED PFCB Fcb, uint64_t Offset, PMDL MdlBuffer, UNUSED uint32_t Flags)
{
	PREP_EXT;
	
	size_t FileSize = Ext->Size;
	size_t Size = MdlBuffer->ByteCount;
	
	// Check for an overflow.
	if (Size + Offset < Size || Size + Offset < Offset)
		return IOSB_STATUS(Iosb, STATUS_INVALID_PARAMETER);
	
	// Ensure a cap over the size of the file.
	if (Offset >= FileSize) {
	SuccessZero:
		Iosb->BytesRead = 0;
		return IOSB_STATUS(Iosb, STATUS_SUCCESS);
	}
	
	if (Size + Offset >= FileSize)
		Size = FileSize - Offset;
	
	if (Size == 0)
		goto SuccessZero;
	
	MmCopyIntoMdl(MdlBuffer, 0, MmGetHHDMOffsetAddr(Ext->Address + Offset), Size);
	
	Iosb->BytesRead = Size;
	return IOSB_STATUS(Iosb, STATUS_SUCCESS);
}

BSTATUS FramebufferWrite(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, PMDL MdlBuffer, UNUSED uint32_t Flags)
{
	PREP_EXT;
	
	size_t FileSize = Ext->Size;
	size_t Size = MdlBuffer->ByteCount;
	
	// Check for an overflow.
	if (Size + Offset < Size || Size + Offset < Offset)
		return IOSB_STATUS(Iosb, STATUS_INVALID_PARAMETER);
	
	// Ensure a cap over the size of the file.
	if (Offset >= FileSize) {
	SuccessZero:
		Iosb->BytesWritten = 0;
		return IOSB_STATUS(Iosb, STATUS_SUCCESS);
	}
	
	if (Size + Offset >= FileSize)
		Size = FileSize - Offset;
	
	if (Size == 0)
		goto SuccessZero;
	
	MmCopyFromMdl(MdlBuffer, 0, MmGetHHDMOffsetAddr(Ext->Address + Offset), Size);
	
	Iosb->BytesWritten = Size;
	return IOSB_STATUS(Iosb, STATUS_SUCCESS);
}

bool FramebufferSeekable(UNUSED PFCB Fcb)
{
	return true;
}

BSTATUS FramebufferBackingMemory(PIO_STATUS_BLOCK Iosb, PFCB Fcb)
{
	PREP_EXT;
	Iosb->BackingMemory.Start = Ext->Address;
	Iosb->BackingMemory.Length = (Ext->Size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	return IOSB_STATUS(Iosb, STATUS_SUCCESS);
}

BSTATUS FramebufferIoControl(
	PFCB Fcb,
	int IoControlCode,
	const void* InBuffer,
	size_t InBufferSize,
	void* OutBuffer,
	size_t OutBufferSize
)
{
	PREP_EXT;
	
	switch (IoControlCode)
	{
		case IOCTL_FRAMEBUFFER_GET_INFO:
		{
			// no data taken in
			if (InBufferSize != 0)
				return STATUS_INVALID_PARAMETER;
			
			// this many bytes put out
			if (OutBufferSize != sizeof(IOCTL_FRAMEBUFFER_INFO))
				return STATUS_INVALID_PARAMETER;
			
			(void) InBuffer;
			
			PIOCTL_FRAMEBUFFER_INFO FbInfo = OutBuffer;
			FbInfo->Width  = Ext->Width;
			FbInfo->Height = Ext->Height;
			FbInfo->Pitch  = Ext->Pitch;
			FbInfo->Bpp    = (short) Ext->Bpp;
			
			return STATUS_SUCCESS;
		}
	}
	
	return STATUS_UNSUPPORTED_FUNCTION;
}

IO_DISPATCH_TABLE FramebufferDispatchTable = {
	.Read = FramebufferRead,
	.Write = FramebufferWrite,
	.IoControl = FramebufferIoControl,
	.Seekable = FramebufferSeekable,
	.BackingMemory = FramebufferBackingMemory,
	.Flags = 0,
};

int GetFrameBufferCount()
{
	return KeLoaderParameterBlock.FramebufferCount;
}

BSTATUS CreateFrameBufferObject(int Index)
{
	BSTATUS Status;
	uintptr_t FbAddress;
	uint32_t FbWidth, FbHeight, FbPitch, FbBpp;
	
	char Name[32];
	snprintf(Name, sizeof Name, "FrameBuffer%d", Index);
	
	PLOADER_FRAMEBUFFER Framebuffer = &KeLoaderParameterBlock.Framebuffers[Index];
	FbAddress = MmGetHHDMOffsetFromAddr(Framebuffer->Address);
	FbWidth  = Framebuffer->Width;
	FbHeight = Framebuffer->Height;
	FbPitch  = Framebuffer->Pitch;
	FbBpp    = Framebuffer->BitDepth;
	
	PDEVICE_OBJECT Device;
	size_t FbSize = FbPitch * FbHeight;
	
	Status = IoCreateDevice(
		FramebufferDriverObject,
		0,                     // DeviceExtensionSize
		sizeof(FCB_EXTENSION), // FcbExtensionSize
		Name,
		DEVICE_TYPE_VIDEO,
		true,                  // Permanent
		&FramebufferDispatchTable,
		&Device
	);
	if (FAILED(Status))
	{
		LogMsg("ERROR: Could not initialize %s object: %s (%d)", Name, RtlGetStatusString(Status), Status);
		return Status;
	}
	
	// Fill in the FCB extension.
	PFCB Fcb = Device->Fcb;
	PREP_EXT;
	
	Ext->Address = FbAddress;
	Ext->Width   = FbWidth;
	Ext->Height  = FbHeight;
	Ext->Pitch   = FbPitch;
	Ext->Bpp     = FbBpp;
	Ext->Size    = FbSize;
	
	// Register the framebuffer address as MMIO
	MmRegisterMMIOAsMemory(FbAddress, FbSize);
	
	// Draw a testing pattern if this isn't the primary framebuffer
	if (Index != 0 && FbBpp == 32)
	{
		size_t RowStart = 0;
		for (uint32_t i = 0; i < FbHeight; i++)
		{
			uint32_t* Row = MmGetHHDMOffsetAddr(FbAddress + RowStart);
			
			for (uint32_t j = 0; j < FbWidth; j++)
				Row[j] = ((i + j) & 1) ? 0xFFFFFF : 0x000000;
			
			RowStart += FbPitch;
		}
	}
	
	return STATUS_SUCCESS;
}

BSTATUS DriverEntry(UNUSED PDRIVER_OBJECT Object)
{
	BSTATUS Status;
	FramebufferDriverObject = Object;
	
	int FrameBufferCount = GetFrameBufferCount();
	for (int i = 0; i < FrameBufferCount; i++)
	{
		Status = CreateFrameBufferObject(i);
		if (FAILED(Status))
			return Status;
	}
	
	return STATUS_SUCCESS;
}
