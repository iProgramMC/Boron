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

BSTATUS NvmePerformIoOperation(
	PIO_STATUS_BLOCK Iosb,
	PFCB Fcb,
	uint64_t Lba,
	uint64_t BlockCount,
	PMDL Mdl,
	bool IsWrite,
	size_t PageOffsetInMdl,
	size_t MdlSizeOverride
)
{
	BSTATUS Status = STATUS_SUCCESS;
	QUEUE_ENTRY_PAIR Qep;
	
	size_t PageCount = Mdl->NumberPages;
	
	// TODO: The IO system needs to restrict this I believe
	ASSERT(PageCount <= 512);
	
	if (MdlSizeOverride != 0)
		PageCount = MdlSizeOverride;
	
	PFCB_EXTENSION FcbExtension = (PFCB_EXTENSION) Fcb->Extension;
	PDEVICE_EXTENSION DeviceExtension = FcbExtension->DeviceExtension;
	
	int BlockSizeLog = DeviceExtension->BlockSizeLog;
	int PrpPfn = PFN_INVALID;
	
	uint64_t Prp[2];
	
	Prp[0] = MmPFNToPhysPage(Mdl->Pages[PageOffsetInMdl]) + Mdl->ByteOffset;
	
	if (PageCount > 2)
	{
		// We will need to allocate an extra page for the PRP list, as the PRP list only takes two pages
		PrpPfn = MmAllocatePhysicalPage();
		if (PrpPfn == PFN_INVALID)
		{
			// Invalid, so we may need to split this write up into parts.
			// TODO: Test me!
			// TESTME
			for (size_t i = 0; i < PageCount; i += 2)
			{
				size_t Min = (i >= PageCount - 2) ? PageCount - i : 2;
				size_t BlocksPerPage = PAGE_SIZE >> BlockSizeLog;
				BSTATUS Status = NvmePerformIoOperation(
					Iosb,
					Fcb,
					Lba + BlocksPerPage * i,
					BlocksPerPage * Min,
					Mdl,
					IsWrite,
					i,
					2
				);
				
				if (FAILED(Status))
					return Status;
			}
			
			return IO_STATUS(Iosb, STATUS_SUCCESS);
		}
		
		// Copy the rest of the pages into the PRP.
		uint64_t* PrpListAddr = MmGetHHDMOffsetAddr(MmPFNToPhysPage(PrpPfn));
		for (size_t Index = 1; Index < PageCount; Index++)
		{
			PrpListAddr[Index - 1] = MmPFNToPhysPage(Mdl->Pages[PageOffsetInMdl + Index]);
		}
		
		Prp[1] = MmPFNToPhysPage(PrpPfn);
	}
	else if (PageCount == 2)
	{
		Prp[1] = MmPFNToPhysPage(Mdl->Pages[PageOffsetInMdl + 1]);
	}
	else
	{
		Prp[1] = 0;
	}
	
	// Finally, send the read/write command.
	if (IsWrite)
		Status = NvmeSendWrite(DeviceExtension, Prp, Lba, BlockCount, true, &Qep);
	else
		Status = NvmeSendRead(DeviceExtension, Prp, Lba, BlockCount, true, &Qep);
	
	// If we had to allocate a PRP list, then free it.
	if (PrpPfn != PFN_INVALID)
		MmFreePhysicalPage(PrpPfn);
	
	return IO_STATUS(Iosb, Status);
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

BSTATUS NvmeRead(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, PMDL Mdl, uint32_t Flags)
{
	ASSERT(Mdl->Flags & MDL_FLAG_WRITE);
	
	size_t Length = Mdl->ByteCount;
	
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
	
	return NvmePerformIoOperation(Iosb, Fcb, Lba, BlockCount, Mdl, false, 0, 0);
}

BSTATUS NvmeWrite(PIO_STATUS_BLOCK Iosb, UNUSED PFCB Fcb, UNUSED uint64_t Offset, UNUSED PMDL Mdl, UNUSED uint32_t Flags)
{
	// TODO
	return Iosb->Status = STATUS_UNIMPLEMENTED;
}

size_t NvmeGetAlignmentInfo(PFCB Fcb)
{
	PFCB_EXTENSION FcbExtension = (PFCB_EXTENSION) Fcb->Extension;
	return 1 << FcbExtension->DeviceExtension->BlockSizeLog;
}
