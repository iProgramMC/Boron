/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	objectst.c
	
Abstract:
	This module, part of the test driver, implements the
	Object Manager test.
	
Author:
	iProgramInCpp - 11 June 2024
***/
#include <ob.h>
#include "tests.h"

// Definitions for the "TestObject" object type.

typedef struct
{
	int Foo;
	int Bar;
	int Baz;
}
TEST_OBJECT, *PTEST_OBJECT;

void TstOpen(void* Object, int HandleCount, OB_OPEN_REASON OpenReason)
{
	PTEST_OBJECT TestObject = Object;
	
	LogMsg(
		"TstOpen: Object %p, handle count %d, open reason %d.  Object->Foo: %d, Object->Bar: %d, Object->Baz: %d",
		Object,
		HandleCount,
		OpenReason,
		TestObject->Foo,
		TestObject->Bar,
		TestObject->Baz
	);
}

void TstClose(void* Object, int HandleCount)
{
	PTEST_OBJECT TestObject = Object;
	
	LogMsg(
		"TstClose: Object %p, handle count %d.  Object->Foo: %d, Object->Bar: %d, Object->Baz: %d",
		Object,
		HandleCount,
		TestObject->Foo,
		TestObject->Bar,
		TestObject->Baz
	);
}

void TstDelete(void* Object)
{
	PTEST_OBJECT TestObject = Object;
	
	LogMsg(
		"TstDelete: Object %p.  Object->Foo: %d, Object->Bar: %d, Object->Baz: %d",
		Object,
		TestObject->Foo,
		TestObject->Bar,
		TestObject->Baz
	);
}

#ifdef DEBUG
void TstDebug(void* Object)
{
	PTEST_OBJECT TestObject = Object;
	
	LogMsg(
		"TstDebug: Object %p.  Object->Foo: %d, Object->Bar: %d, Object->Baz: %d",
		Object,
		TestObject->Foo,
		TestObject->Bar,
		TestObject->Baz
	);
}
#endif

OBJECT_TYPE_INFO TstObjectTypeInfo =
{
	.InvalidAttributes = 0,
	.ValidAccessMask = 0,
	.NonPagedPool = false,
	.MaintainHandleCount = true,
	.Open = NULL,
	.Close = NULL,
	.Delete = NULL,
	.Parse = NULL,
	.Secure = NULL,
#ifdef DEBUG
	.Debug = NULL,
#endif
};

static void InitializeTstObjectTypeInfo()
{
	TstObjectTypeInfo.Open = TstOpen;
	TstObjectTypeInfo.Close = TstClose;
	TstObjectTypeInfo.Delete = TstDelete;
}

POBJECT_TYPE TstObjectType;

// System object type test:
POBJECT_TYPE TstDirectoryType, TstSymbolicLinkType, TstObjectTypeType;

static void AttemptFetchObjectTypes()
{
	BSTATUS Status;
	
	void* Object = NULL;
	Status = ObReferenceObjectByName("\\ObjectTypes\\Type", NULL, 0, NULL, &Object);
	if (FAILED(Status))
	{
		LogMsg("ERROR: Object test failed with error %d. Could not locate \\ObjectTypes\\Type", Status);
		ASSERT(false);
	}
	
	TstObjectTypeType = Object;
	
	Status = ObReferenceObjectByName("\\ObjectTypes\\SymbolicLink", NULL, 0, TstObjectTypeType, &Object);
	if (FAILED(Status))
	{
		LogMsg("ERROR: Object test failed with error %d. Could not locate \\ObjectTypes\\SymbolicLink", Status);
		ASSERT(false);
	}
	
	TstSymbolicLinkType = Object;
	
	Status = ObReferenceObjectByName("\\ObjectTypes\\SymbolicLink", NULL, 0, TstObjectTypeType, &Object);
	if (FAILED(Status))
	{
		LogMsg("ERROR: Object test failed with error %d. Could not locate \\ObjectTypes\\DirectoryType", Status);
		ASSERT(false);
	}
	
	TstDirectoryType = Object;
}

static void AttemptCreateSymLink()
{
	BSTATUS Status;
	
	// Try creating a symbolic link:
	void *Symlink = NULL;
	Status = ObCreateSymbolicLinkObject(
		&Symlink,
		"\\ObjectTypes",
		"\\ObjectTypes\\Test Link",
		OB_FLAG_KERNEL
	);
	
	if (FAILED(Status))
	{
		LogMsg("ERROR: Can't create symlink: %d", Status);
		ASSERT(false);
		return;
	}
	
	const char* TestPath = "\\ObjectTypes\\Test Link\\Test Link\\Test Link\\Directory";
	
	// Try looking up a certain path:
	void* Obj = NULL;
	Status = ObReferenceObjectByName(
		TestPath,
		NULL,              // InitialParseObject
		0,                 // OpenFlags
		TstObjectTypeType, // ExpectedType
		&Obj
	);
	
	if (FAILED(Status))
	{
		LogMsg("ERROR: Look up of path '%s' failed with error: %d", TestPath, Status);
	}
	else
	{
		LogMsg("Look up of path '%s' succeeded.", TestPath);
			
		LogMsg("It returned object %p with type %p", Obj, OBJECT_GET_HEADER(Obj)->NonPagedObjectHeader->ObjectType);
		
		LogMsg(
			"Init types: ObpObjectTypeType: %p, ObpDirectoryType: %p, ObpSymbolicLinkType: %p",
			TstObjectTypeType,
			TstDirectoryType,
			TstSymbolicLinkType
		);
	}
}

void AttemptCreateTestType()
{
	InitializeTstObjectTypeInfo();
	
	BSTATUS Status;
	
	Status = ObCreateObjectType(
		"TestObject",
		&TstObjectTypeInfo,
		&TstObjectType
	);
	
	if (FAILED(Status))
		KeCrash("Failed to create 'TestObject' object type with error %d", Status);
	
	// Ok, now perform a global lookup, see if it works.
	HANDLE Handle = HANDLE_NONE;
	Status = ObOpenObjectByName("\\ObjectTypes\\TestObject", HANDLE_NONE, 0, TstObjectTypeType, &Handle);
	
	if (FAILED(Status))
		KeCrash("Failed to find 'TestObject' in object types directory with error %d", Status);
	
	// Reference that object by handle.
	void* Object;
	Status = ObReferenceObjectByHandle(Handle, TstObjectTypeType, &Object);
	
	if (FAILED(Status))
		KeCrash("Failed to reference object by handle %d, error %d", Handle, Status);
	
	if (Object != (void*) TstObjectType)
		KeCrash("\\ObjectTypes\\TestObject returned something (%p) other than TstObjectType (%p)", Object, TstObjectType);
	
	// Now, dereference the object, and close the handle.
	ObDereferenceObject(Object);
	
	Status = ObClose(Handle);
	if (FAILED(Status))
		KeCrash("Failed to close handle %d, error %d", Handle, Status);
}

void ModifyEphemeralObject(void* _Object)
{
	BSTATUS Status;
	HANDLE Handle;
	
	Status = ObInsertObject(_Object, &Handle);
	if (FAILED(Status))
		KeCrash("ModifyEphemeralObject: Cannot ObInsertObject(%p): error %d", _Object, Status);
	
	LogMsg("ModifyEphemeralObject: Handle is %d", Handle);
	
	void* ObjectV;
	Status = ObReferenceObjectByHandle(Handle, TstObjectType, &ObjectV);
	if (FAILED(Status))
		KeCrash("ModifyEphemeralObject: Cannot ObReferenceObjectByHandle(%d): error %d", Handle, Status);
	
	PTEST_OBJECT Object = ObjectV;
	if (Object != _Object)
		KeCrash("ModifyEphemeralObject: ObReferenceObjectByHandle returned %p for object %p, handle %d", Object, _Object, Status);
	
	// Assign some new values to the object.
	Object->Foo = 1357;
	Object->Bar = 2468;
	Object->Baz = 3579;
	
	ObDereferenceObject(Object);
	ObClose(Handle);
}

void AttemptCreateEphemeralObject()
{
	BSTATUS Status;
	void* Object;
	
	// Create an object of the type "TestObject". It won't be attached to a directory.
	Status = ObCreateObject(
		&Object,
		NULL,
		TstObjectType,
		"My Test Object",
		OB_FLAG_KERNEL | OB_FLAG_NO_DIRECTORY,
		NULL,
		sizeof(TEST_OBJECT)
	);
	if (FAILED(Status))
		KeCrash("Could not create 'My Test Object': error %d", Status);
	
	// Assign some properties to this object
	PTEST_OBJECT TestObject = Object;
	TestObject->Foo = 1234;
	TestObject->Bar = 5678;
	TestObject->Baz = 9012;
	
	ModifyEphemeralObject(Object);
	
	// Remove the final reference at high IPL.  It should go through the reaper thread and delete.
	KIPL Ipl = KeRaiseIPL(IPL_DPC);
	DbgPrint("ModifyEphemeralObject: Deleting final reference at high IPL");
	ObDereferenceObject(Object);
	KeLowerIPL(Ipl);
}

void PerformObjectTest()
{
	AttemptFetchObjectTypes();
	AttemptCreateSymLink();
	AttemptCreateTestType();
	AttemptCreateEphemeralObject();
}
