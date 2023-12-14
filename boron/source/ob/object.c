/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/object.c
	
Abstract:
	This module implements common functions to handle objects.
	
Author:
	iProgramInCpp - 8 December 2023
***/
#include "obp.h"

#ifdef DEBUG
BSTATUS ObiDebugObject(void* Object)
{
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	POBJECT_TYPE Type = Hdr->NonPagedObjectHeader->ObjectType;
	
	if (!Type->TypeInfo.Debug)
		return STATUS_SUCCESS; // Unimplemented.
	
	return Type->TypeInfo.Debug(Object);
}
#endif

BSTATUS ObpDeleteObject(void* Object)
{
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	POBJECT_TYPE Type = Hdr->NonPagedObjectHeader->ObjectType;
	
	if (Type->TypeInfo.Delete)
		Type->TypeInfo.Delete(Object);
	
	PNONPAGED_OBJECT_HEADER NpHdr = Hdr->NonPagedObjectHeader;
	
	if (Type->TypeInfo.NonPagedPool)
	{
		MmFreePool(NpHdr);
	}
	else
	{
		MmFreePool(NpHdr);
		MmFreePool(Hdr);
	}
	
	return STATUS_SUCCESS;
}

extern POBJECT_DIRECTORY ObpRootDirectory;

BSTATUS ObiLookUpObject(void* ParseObject, const char* Name, void** OutObject)
{
	*OutObject = NULL;
	
	if (*Name == '\0')
		return STATUS_NAME_INVALID;
	
	if (!ParseObject)
	{
		if (*Name != OB_PATH_SEPARATOR)
			return STATUS_PATH_INVALID;
		
		ParseObject = ObpRootDirectory;
		Name++;
	}
	
	bool UsingInitialParseObject = true;
	
	// Add reference to initial parse object.  This is so that
	// we don't have to add a special case when decrementing the
	// reference count to the parse object.
	ObpAddReferenceToObject(ParseObject);
	
	while (*Name != '\0')
	{
		POBJECT_HEADER Hdr = OBJECT_GET_HEADER(ParseObject);
		POBJECT_TYPE Type  = Hdr->NonPagedObjectHeader->ObjectType;
		void* ParseContext = Hdr->ParseContext;
		
		if (!Type->TypeInfo.Parse)
		{
			ObiDereferenceObject(ParseObject);
			
			// If using the initial parse object, the 'parse' isn't supported on that.
			// If not, the parse isn't supported on a child, so we matched an object,
			// but there were more characters to match.
			
			if (UsingInitialParseObject)
				return STATUS_UNSUPPORTED_FUNCTION;
			else
				return STATUS_PATH_INVALID;
		}
		
		void* Object = NULL;
		
		BSTATUS Status = Type->TypeInfo.Parse(
			ParseObject,
			&Name,
			ParseContext,
			&Object
		);
		
		if (FAILED(Status))
		{
			ObiDereferenceObject(ParseObject);
			return Status;
		}
		
		*OutObject = Object;
		
		ObpAddReferenceToObject(Object);
		ObiDereferenceObject(ParseObject);
		
		ParseObject = Object;
		
		UsingInitialParseObject = false;
	}
	
	return STATUS_SUCCESS;
}
