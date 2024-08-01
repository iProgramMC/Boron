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

// remove this as it's not really needed
BSTATUS NvmePerformIoOperationPagingIo(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Lba, uint64_t BlockCount, void* BufferDst, const void* BufferSrc, bool IsWrite)
{
	PFCB_EXTENSION FcbExtension = (PFCB_EXTENSION) Fcb->Extension;
	PDEVICE_EXTENSION DeviceExtension = FcbExtension->DeviceExtension;
	
	int BlockSizeLog = DeviceExtension->BlockSizeLog;
	ASSERT((BlockCount << BlockSizeLog) <= DeviceExtension->ContExtension->MaximumDataTransferSize);
	
	PKMUTEX RioMutex = &DeviceExtension->ReserveIoMutex;
	BSTATUS Status = KeWaitForSingleObject(RioMutex, true, TIMEOUT_INFINITE);
	if (FAILED(Status))
		return IO_STATUS(Iosb, Status);
	
	// Use that reserve I/O page to perform the op on the drive.
	// First, do pagefuls at a time.
	size_t BlocksPerIter = PAGE_SIZE >> BlockSizeLog;
	size_t TotalBlocksProcessed = 0;
	
	uint64_t EndLba = Lba + BlockCount;
	
	uintptr_t ReserveIoPage = MmPFNToPhysPage(DeviceExtension->ReserveIoPagePfn);
	void* ReserveIoMem = MmGetHHDMOffsetAddr(ReserveIoPage);
	
	const uint8_t*  InBuffer = BufferSrc;
	/***/ uint8_t* OutBuffer = BufferDst;
	
	QUEUE_ENTRY_PAIR Qep;
	
	uint64_t Prp[2];
	Prp[0] = ReserveIoPage;
	Prp[1] = 0;
	
	for (; Lba + BlocksPerIter <= EndLba; Lba += BlocksPerIter)
	{
		if (IsWrite)
		{
			memcpy(ReserveIoMem, InBuffer, PAGE_SIZE);
			InBuffer += PAGE_SIZE;
			
			Status = NvmeSendWrite(DeviceExtension, Prp, Lba, BlocksPerIter, true, &Qep);
		}
		else
		{
			Status = NvmeSendRead(DeviceExtension, Prp, Lba, BlocksPerIter, true, &Qep);
		}
		
		if (FAILED(Status))
		{
			KeReleaseMutex(RioMutex);
			
			ASSERT(offsetof(IO_STATUS_BLOCK, BytesRead) == offsetof(IO_STATUS_BLOCK, BytesWritten));
			
			Iosb->BytesRead = TotalBlocksProcessed << BlockSizeLog;
			return IO_STATUS(Iosb, Status);
		}
		
		TotalBlocksProcessed += BlocksPerIter;
		BlockCount -= BlocksPerIter;
		
		if (!IsWrite)
		{
			memcpy(OutBuffer, ReserveIoMem, PAGE_SIZE);
			OutBuffer += PAGE_SIZE;
		}
	}
	
	// Finally, do the last piece, if needed.
	if (BlockCount != 0)
	{
		if (IsWrite)
		{
			memcpy(ReserveIoMem, InBuffer, BlockCount << BlockSizeLog);
			TotalBlocksProcessed += BlockCount;
			
			Status = NvmeSendWrite(DeviceExtension, Prp, Lba, BlockCount, true, &Qep);
		}
		else
		{
			Status = NvmeSendRead(DeviceExtension, Prp, Lba, BlockCount, true, &Qep);
		}
		
		if (FAILED(Status))
		{
			KeReleaseMutex(RioMutex);
			Iosb->BytesRead = TotalBlocksProcessed << BlockSizeLog;
			return IO_STATUS(Iosb, Status);
		}
		
		if (!IsWrite)
		{
			memcpy(OutBuffer, ReserveIoMem, BlockCount << BlockSizeLog);
			TotalBlocksProcessed += BlockCount;
		}
	}
	
	ASSERT(offsetof(IO_STATUS_BLOCK, BytesRead) == offsetof(IO_STATUS_BLOCK, BytesWritten));
	
	KeReleaseMutex(RioMutex);
	Iosb->BytesRead = TotalBlocksProcessed << BlockSizeLog;
	return IO_STATUS(Iosb, STATUS_SUCCESS);
}

BSTATUS NvmePerformIoOperation(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Lba, uint64_t BlockCount, void* BufferDst, const void* BufferSrc, bool IsWrite)
{
	// TODO
	return NvmePerformIoOperationPagingIo(Iosb, Fcb, Lba, BlockCount, BufferDst, BufferSrc, IsWrite);
}

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
	
	if (Flags & IO_RW_PAGING)
		return NvmePerformIoOperationPagingIo(Iosb, Fcb, Lba, BlockCount, Buffer, NULL, false);
	else
		return NvmePerformIoOperation(Iosb, Fcb, Lba, BlockCount, Buffer, NULL, false);
	
	// TODO
	return IO_STATUS(Iosb, STATUS_UNIMPLEMENTED);
}

BSTATUS NvmeWrite(PIO_STATUS_BLOCK Iosb, UNUSED PFCB Fcb, UNUSED uint64_t Offset, UNUSED uint64_t Length, UNUSED const void* Buffer, UNUSED uint32_t Flags)
{
	// TODO
	return Iosb->Status = STATUS_UNIMPLEMENTED;
}

size_t NvmeGetAlignmentInfo(PFCB Fcb)
{
	PFCB_EXTENSION FcbExtension = (PFCB_EXTENSION) Fcb->Extension;
	return 1 << FcbExtension->DeviceExtension->BlockSizeLog;
}
