//
//  File.cpp
//
//  Copyright (C) 2026 iProgramInCpp
//  Created on 9/4/2026.
//
#include "File.hpp"

File::File(const char* ObjectName, int OpenFlags, HANDLE RootDirectory)
{
	OBJECT_ATTRIBUTES Attributes {};
	Attributes.ObjectName = ObjectName;
	Attributes.ObjectNameLength = strlen(ObjectName);
	Attributes.OpenFlags = OpenFlags;
	Attributes.RootDirectory = RootDirectory;
	
	m_bOwningHandle = true;
	m_Handle = HANDLE_NONE;
	m_OpenStatus = OSOpenFile(&m_Handle, &Attributes);
}

File::File(HANDLE Handle)
{
	m_Handle = Handle;
	m_bOwningHandle = false;
}

File::~File()
{
	if (m_Handle != HANDLE_NONE && m_bOwningHandle)
		OSClose(m_Handle);
}

BSTATUS File::GetOpenStatus() const
{
	return m_OpenStatus;
}

HANDLE File::GetHandle() const
{
	return m_Handle;
}

HANDLE File::PullHandle()
{
	m_bOwningHandle = false;
	return m_Handle;
}

void File::SetSharedRWOffset(bool UseSharedRWOffset)
{
	m_bUseSharedRWOffset = UseSharedRWOffset;
}

BSTATUS File::Read(PIO_STATUS_BLOCK Iosb, void* Buffer, size_t Length, uint64_t Offset, uint32_t Flags)
{
	if (m_bUseSharedRWOffset)
		Flags |= IO_RW_SHARED_FILE_OFFSET;
	else
		Flags &= ~IO_RW_SHARED_FILE_OFFSET;
	
	IO_STATUS_BLOCK IosbTemp {};
	if (!Iosb)
		Iosb = &IosbTemp;
	
	return OSReadFile(Iosb, m_Handle, Offset, Buffer, Length, Flags);
}

BSTATUS File::Write(PIO_STATUS_BLOCK Iosb, const void* Buffer, size_t Length, uint64_t Offset, uint32_t Flags, uint64_t* NewFileSize)
{
	if (m_bUseSharedRWOffset)
		Flags |= IO_RW_SHARED_FILE_OFFSET;
	else
		Flags &= ~IO_RW_SHARED_FILE_OFFSET;
	
	IO_STATUS_BLOCK IosbTemp {};
	if (!Iosb)
		Iosb = &IosbTemp;
	
	return OSWriteFile(Iosb, m_Handle, Offset, Buffer, Length, Flags, NewFileSize);
}

BSTATUS File::Seek(int64_t Offset, int Whence, uint64_t* NewOutOffset)
{
	return OSSeekFile(m_Handle, Offset, Whence, NewOutOffset);
}

BSTATUS File::MapView(void** BaseAddressInOut, size_t ViewSize, int AllocationType, int Protection, uint64_t SectionOffset, HANDLE ProcessHandle)
{
	return OSMapViewOfObject(
		ProcessHandle,
		m_Handle,
		BaseAddressInOut,
		ViewSize,
		AllocationType,
		SectionOffset,
		Protection
	);
}

BSTATUS File::DeviceIoControl(int IoControlCode, void* InBuffer, size_t InBufferSize, void* OutBuffer, size_t OutBufferSize)
{
	return OSDeviceIoControl(
		m_Handle,
		IoControlCode,
		InBuffer,
		InBufferSize,
		OutBuffer,
		OutBufferSize
	);
}
