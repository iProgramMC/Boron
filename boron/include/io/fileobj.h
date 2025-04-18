/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/fileobj.h
	
Abstract:
	This header defines the structure of the opened file object.
	
Author:
	iProgramInCpp - 22 June 2024
***/
#pragma once

enum
{
	FILE_FLAG_APPEND_ONLY = (1 << 0),
};

typedef struct _FILE_OBJECT
{
	PFCB Fcb;
	
	void* Context1;
	void* Context2;
	
	uint64_t Offset;
	uint32_t Flags;
	uint32_t OpenFlags;
}
FILE_OBJECT, *PFILE_OBJECT;

bool IoIsSeekable(PFILE_OBJECT Object);
