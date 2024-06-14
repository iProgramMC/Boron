#pragma once

#include <io.h>
#include <mm.h>
#include <ob.h>
#include <ldr.h>

extern POBJECT_TYPE IopDriverType, IopDeviceType, IopFileType;

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
