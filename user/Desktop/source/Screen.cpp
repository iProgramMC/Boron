//
//  Screen.cpp
//
//  Copyright (C) 2026 iProgramInCpp.
//  Created on 9/4/2026.
//
#include "Screen.hpp"

Screen::Screen(HANDLE FramebufferHandle) : m_FramebufferFile(FramebufferHandle)
{
	BSTATUS Status;
	void* FbAddress;
	size_t Size;
	
	// Grab the frame buffer info.
	IOCTL_FRAMEBUFFER_INFO FbInfo;
	Status = m_FramebufferFile.DeviceIoControl(
		IOCTL_FRAMEBUFFER_GET_INFO,
		NULL,
		0,
		&FbInfo,
		sizeof FbInfo
	);
	if (FAILED(Status))
	{
		DbgPrint("ERROR: Failed to get framebuffer info. %s", RtlGetStatusString(Status));
		goto End;
	}
	
	FbAddress = NULL;
	Size = FbInfo.Pitch * FbInfo.Height;
	Status = m_FramebufferFile.MapView(
		&FbAddress,
		Size,
		MEM_COMMIT,
		PAGE_READ | PAGE_WRITE
	);
	
	if (FAILED(Status))
	{
		DbgPrint("ERROR: Could not map framebuffer to memory. %s", RtlGetStatusString(Status));
		goto End;
	}
	
	m_Context = CGCreateContextFromBuffer(
		FbAddress,
		(int) FbInfo.Width,
		(int) FbInfo.Height,
		FbInfo.Pitch,
		FbInfo.Bpp,
		FbInfo.ColorFormat
	);
	
	if (!m_Context)
	{
		DbgPrint("ERROR: Failed to create graphics context.");
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto End;
	}
	
End:
	m_CreateStatus = Status;
}

Screen::~Screen()
{
	CGFreeContext(m_Context);
}


