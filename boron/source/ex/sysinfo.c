/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ex/sysinfo.c
	
Abstract:
	This module implements the OSQuerySystemInformation system service.
	It allows miscellaneous information about the system to be obtained,
	such as a list of running processes.
	
Author:
	iProgramInCpp - 4 March 2026
***/
#include <ex.h>
#include <ps.h>
#include <mm.h>
#include <string.h>

#define MIN_BUFFER_SIZE (1024)
#define MAX_BUFFER_SIZE (1 * 1024 * 1024)

static BSTATUS ExpQueryBasicInformation(void* Buffer, size_t BufferSize, size_t* WrittenBufferSize);
static BSTATUS ExpQueryMemoryInformation(void* Buffer, size_t BufferSize, size_t* WrittenBufferSize);

BSTATUS OSQuerySystemInformation(
	uint32_t QueryType,
	void* UserBuffer,
	size_t UserBufferSize,
	size_t* SizeOfReturnedDataOut
)
{
	BSTATUS Status;
	if (UserBufferSize > MAX_BUFFER_SIZE || UserBufferSize < sizeof(short))
	{
		// you really don't need a bigger size than 1 MB, or a smaller size than a short.
		return STATUS_INVALID_PARAMETER;
	}
	
	if (QueryType >= QUERY_MAXIMUM)
	{
		// We don't support this query type.
		// Return unsupported function instead of invalid parameter, because we may add
		// this query type in the future.
		return STATUS_UNSUPPORTED_FUNCTION;
	}
	
	// This has the effect of truncating the data in case the user provides a smaller
	// buffer than required.  However, this is an acceptable trade-off, as the user will be
	// informed of both whatever data it supported accessing back when that version of the API
	// looked like at the time, *and* the new size of the structure.
	size_t BufferSize = UserBufferSize;
	if (BufferSize < MIN_BUFFER_SIZE)
		BufferSize = MIN_BUFFER_SIZE;
	
	void* Buffer = MmAllocatePool(POOL_PAGED, UserBufferSize);
	if (!Buffer)
	{
		// allocation failed!
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	memset(Buffer, 0, sizeof Buffer);
	
	size_t SizeOfReturnedData = 0;
	switch (QueryType)
	{
		case QUERY_BASIC_INFORMATION:
			Status = ExpQueryBasicInformation(Buffer, BufferSize, &SizeOfReturnedData);
			break;
		
		case QUERY_MEMORY_INFORMATION:
			Status = ExpQueryMemoryInformation(Buffer, BufferSize, &SizeOfReturnedData);
			break;
		
		case QUERY_PROCESS_INFORMATION:
			// Return process information here.
			Status = STATUS_UNIMPLEMENTED;
			break;
		
		case QUERY_THREAD_INFORMATION:
			// Return thread information here.
			Status = STATUS_UNIMPLEMENTED;
			break;
	}

	if (SizeOfReturnedData > UserBufferSize)
		SizeOfReturnedData = UserBufferSize;

	if (FAILED(Status))
		goto EarlyReturn;
	
	// Copy the data back in.
	Status = MmSafeCopy(
		SizeOfReturnedDataOut,
		&SizeOfReturnedData,
		sizeof(size_t),
		KeGetPreviousMode(),
		true
	);
	
	if (FAILED(Status))
		goto EarlyReturn;
	
	if (SizeOfReturnedData != 0)
		Status = MmSafeCopy(UserBuffer, Buffer, SizeOfReturnedData, KeGetPreviousMode(), true);

EarlyReturn:
	MmFreePool(Buffer);
	return Status;
}




static BSTATUS ExpQueryBasicInformation(void* Buffer, size_t BufferSize, size_t* WrittenBufferSize)
{
	if (BufferSize < sizeof(SYSTEM_BASIC_INFORMATION))
		return STATUS_FAULT;
	
	PSYSTEM_BASIC_INFORMATION BasicInfo = Buffer;
	BasicInfo->Size = sizeof(*BasicInfo);
	
	BasicInfo->OSVersionNumber = KeGetVersionNumber();
	BasicInfo->PageSize = PAGE_SIZE;
	BasicInfo->ProcessorCount = KeGetProcessorCount();
	BasicInfo->AllocationGranularity = PAGE_SIZE;
	BasicInfo->MinimumUserModeAddress = MM_FIRST_USER_PAGE;
	BasicInfo->MaximumUserModeAddress = MM_LAST_USER_PAGE;
	memset(BasicInfo->OSName, 0, sizeof BasicInfo->OSName);
	strcpy(BasicInfo->OSName, "Boron");
	
	*WrittenBufferSize = sizeof(*BasicInfo);
	return STATUS_SUCCESS;
}

static BSTATUS ExpQueryMemoryInformation(void* Buffer, size_t BufferSize, size_t* WrittenBufferSize)
{
	if (BufferSize < sizeof(SYSTEM_MEMORY_INFORMATION))
		return STATUS_FAULT;
	
	PSYSTEM_MEMORY_INFORMATION MemoryInfo = Buffer;
	MemoryInfo->Size = sizeof(*MemoryInfo);
	
	MemoryInfo->PageSize = PAGE_SIZE;
	MemoryInfo->TotalPhysicalMemoryPages = MmGetTotalAvailablePages();
	MemoryInfo->FreePhysicalMemoryPages = MmGetTotalFreePages();
	
	*WrittenBufferSize = sizeof(*MemoryInfo);
	return STATUS_SUCCESS;
}
