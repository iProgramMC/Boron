#pragma once

#include <io.h>
#include <mm.h>
#include <ob.h>
#include <ldr.h>

extern POBJECT_TYPE IoDriverType, IoDeviceType, IoFileType;

// Driver object operations
void IopDeleteDriver(void* Object);

// Device object operations
void IopDeleteDevice(void* Object);
BSTATUS IopParseDevice(void* Object, const char** Name, void* Context, int LoopCount, void** OutObject);

// File object operations
void IopDeleteFile(void* Object);
void IopCloseFile(void* Object, int HandleCount);
BSTATUS IopParseFile(void* Object, const char** Name, void* Context, int LoopCount, void** OutObject);

bool IopInitializeDevicesDir();
bool IopInitializeDriversDir();
bool IopInitializeDeviceType();
bool IopInitializeDriverType();
bool IopInitializeFileType();

// Create a file object. This doesn't actually open the object.
BSTATUS IopCreateFileObject(PFCB Fcb, PFILE_OBJECT* OutObject, uint32_t Flags, uint32_t OpenFlags);

BSTATUS IopCreateDeviceFileObject(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT* OutObject, uint32_t Flags, uint32_t OpenFlags);
