//
//  File.hpp
//
//  Copyright (C) 2026 iProgramInCpp
//  Created on 9/4/2026.
//
#pragma once

#include <boron.h>

class File
{
public:
	File(const char* ObjectName, int OpenFlags = 0, HANDLE RootDirectory = HANDLE_NONE);
	File(HANDLE Handle);
	~File();
	
	BSTATUS GetOpenStatus() const;
	HANDLE GetHandle() const;
	HANDLE PullHandle();
	
	void SetSharedRWOffset(bool UseSharedRWOffset);
	
	BSTATUS Read(
		PIO_STATUS_BLOCK Iosb,
		void* Buffer,
		size_t Length,
		uint64_t Offset = 0,
		uint32_t Flags = 0
	);
	BSTATUS Write(
		PIO_STATUS_BLOCK Iosb,
		const void* Buffer,
		size_t Length,
		uint64_t Offset = 0,
		uint32_t Flags = 0,
		uint64_t* NewFileSize = nullptr
	);
	BSTATUS MapView(
		void** BaseAddressInOut,
		size_t ViewSize,
		int AllocationType,
		int Protection,
		uint64_t SectionOffset = 0,
		HANDLE ProcessHandle = CURRENT_PROCESS_HANDLE
	);
	BSTATUS Seek(
		int64_t Offset,
		int Whence,
		uint64_t* NewOutOffset = nullptr
	);
	BSTATUS DeviceIoControl(
		int IoControlCode,
		void* InBuffer,
		size_t InBufferSize,
		void* OutBuffer,
		size_t OutBufferSize
	);
	
private:
	HANDLE m_Handle;
	bool m_bOwningHandle = false;
	bool m_bUseSharedRWOffset = false;
	BSTATUS m_OpenStatus;
};
