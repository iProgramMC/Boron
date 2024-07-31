/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	commands.c
	
Abstract:
	This module implements the I/O manager facing read/write
	functions for the NVMe disk driver.
	
Author:
	iProgramInCpp - 28 July 2024
***/
#include "nvme.h"
#include <string.h>

#define IO_STATUS(Iosb, Stat) ((Iosb)->Status = (Stat))

// NOTE for reading and writing:
//
// - If reading 1 page, DataPointer[0] is used and point to physical pages which are written to by the controller
//
// - If reading 2 pages, DataPointer[0] and DataPointer[1] are used and ditto
//
// - If reading more than 2 pages' worth of blocks, DataPointer[0] is used and points to
//   a physical page written to by the controller, and DataPointer[1] points to a physical
//   page which contains a list of physical pages written to by the controller.

BSTATUS NvmeRead(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, uint64_t Length, void* Buffer, uint32_t Flags)
{
	PFCB_EXTENSION FcbExtension = (PFCB_EXTENSION) Fcb->Extension;
	PDEVICE_EXTENSION DeviceExtension = FcbExtension->DeviceExtension;
	
	int BlockSizeLog = DeviceExtension->BlockSizeLog;
	int BlockSize    = DeviceExtension->BlockSize;
	uint64_t Mask    = BlockSize - 1;
	
	// If the offset or the length are unaligned, then return out an unaligned I/O attempt error.
	if ((Offset & Mask) || (Length & Mask))
		return Iosb->Status = STATUS_UNALIGNED_OPERATION;
	
	uint64_t Lba = Offset >> BlockSizeLog;
	uint64_t BlockCount = Length >> BlockSizeLog;
	
	if (!BlockCount)
		// No I/O to be done actually
		return IO_STATUS(Iosb, STATUS_SUCCESS);
	
	// The NVMe driver does not take into account non-block read/write operations, because the NVMe controller
	// cannot instantly reply to requests from us.
	if (Flags & IO_RW_NONBLOCK)
		return IO_STATUS(Iosb, STATUS_INVALID_PARAMETER);
	
	uint64_t PageCount = ((BlockCount << BlockSizeLog) + PAGE_SIZE - 1) / PAGE_SIZE;
	
	uint64_t EndLba = Lba + BlockCount;
	
	uint64_t Prp[2];
	
	// TODO: do this only if
	// if (Flags & IO_RW_PAGINGIO)
	{
		PKMUTEX ReadMutex = &DeviceExtension->ReserveReadMutex;
		BSTATUS Status = KeWaitForSingleObject(ReadMutex, true, TIMEOUT_INFINITE);
		if (FAILED(Status))
			return IO_STATUS(Iosb, Status);
		
		// Use that reserve read page to read from the drive.
		// First, read pagefuls at a time.
		size_t ReadPerIter = PAGE_SIZE >> BlockSizeLog;
		size_t TotalBlocksRead = 0;
		
		uintptr_t ReserveReadPage = MmPFNToPhysPage(DeviceExtension->ReserveReadPagePfn);
		void* ReserveReadMem = MmGetHHDMOffsetAddr(ReserveReadPage);
		
		uint8_t* OutBuffer = Buffer;
		
		QUEUE_ENTRY_PAIR Qep;
		
		Prp[0] = ReserveReadPage;
		Prp[1] = 0;
		
		for (; Lba + ReadPerIter <= EndLba; Lba += ReadPerIter)
		{
			Status = NvmeSendRead(DeviceExtension, Prp, Lba, ReadPerIter, true, &Qep);
			if (FAILED(Status))
			{
				KeReleaseMutex(ReadMutex);
				Iosb->BytesRead = TotalBlocksRead << BlockSizeLog;
				return IO_STATUS(Iosb, Status);
			}
			
			TotalBlocksRead += ReadPerIter;
			BlockCount -= ReadPerIter;
			
			memcpy(OutBuffer, ReserveReadMem, PAGE_SIZE);
			OutBuffer += PAGE_SIZE;
		}
		
		// Finally, read the last piece, if needed.
		if (BlockCount != 0)
		{
			Status = NvmeSendRead(DeviceExtension, Prp, Lba, BlockCount, true, &Qep);
			if (FAILED(Status))
			{
				KeReleaseMutex(ReadMutex);
				Iosb->BytesRead = TotalBlocksRead << BlockSizeLog;
				return IO_STATUS(Iosb, Status);
			}
			
			memcpy(OutBuffer, ReserveReadMem, BlockCount << BlockSizeLog);
			TotalBlocksRead += BlockCount;
		}
		
		KeReleaseMutex(ReadMutex);
		DbgPrint("Yay!");
		Iosb->BytesRead = TotalBlocksRead << BlockSizeLog;
		return IO_STATUS(Iosb, STATUS_SUCCESS);
	}
	
	
	// TODO
	return IO_STATUS(Iosb, STATUS_UNIMPLEMENTED);
}

BSTATUS NvmeWrite(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, uint64_t Length, const void* Buffer, uint32_t Flags)
{
	// TODO
	return Iosb->Status = STATUS_UNIMPLEMENTED;
}

size_t NvmeGetAlignmentInfo(PFCB Fcb)
{
	PFCB_EXTENSION FcbExtension = (PFCB_EXTENSION) Fcb->Extension;
	return 1 << FcbExtension->DeviceExtension->BlockSizeLog;
}
