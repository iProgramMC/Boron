#include <boron.h>
#include <string.h>
#include "misc.h"

void DumpHex(void* DataV, size_t DataSize)
{
	uint8_t* Data = DataV;
	
	#define A(x) (((x) >= 0x20 && (x) <= 0x7F) ? (x) : '.')
	
	const size_t PrintPerRow = 32;
	
	for (size_t i = 0; i < DataSize; i += PrintPerRow) {
		char Buffer[256];
		Buffer[0] = 0;
		
		//sprintf(Buffer + strlen(Buffer), "%04lx: ", i);
		sprintf(Buffer + strlen(Buffer), "%p: ", Data + i);
		
		for (size_t j = 0; j < PrintPerRow; j++) {
			if (i + j >= DataSize)
				strcat(Buffer, "   ");
			else
				sprintf(Buffer + strlen(Buffer), "%02x ", Data[i + j]);
		}
		
		strcat(Buffer, "  ");
		
		for (size_t j = 0; j < PrintPerRow; j++) {
			if (i + j >= DataSize)
				strcat(Buffer, " ");
			else
				sprintf(Buffer + strlen(Buffer), "%c", A(Data[i + j]));
		}
		
		DbgPrint("%s", Buffer);
	}
}

void FileList(const char* Path, size_t StartOffset, size_t Size, size_t SizePrint)
{
	LogMsg("Reading contents of %s, start offset %zu, size %zu, size printed %zu", Path, StartOffset, Size, SizePrint);
	
	HANDLE Handle;
	BSTATUS Status;
	IO_STATUS_BLOCK Iosb;
	
	OBJECT_ATTRIBUTES Attrs;
	memset(&Attrs, 0, sizeof Attrs);
	Attrs.RootDirectory = HANDLE_NONE;
	Attrs.ObjectName = Path;
	Attrs.ObjectNameLength = strlen(Path);
	Attrs.OpenFlags = 0;
	
	Status = OSOpenFile(&Handle, &Attrs);
	if (FAILED(Status))
		CRASH("Fs1: Failed to open %s: %d (%s)", Path, Status, RtlGetStatusString(Status));
	
	void* Buffer = OSAllocate(Size);
	if (!Buffer)
		CRASH("Fs1: Out of memory, cannot allocate 4K buffer");
	
	Status = OSReadFile(&Iosb, Handle, StartOffset, Buffer, Size, 0);
	if (FAILED(Status))
		CRASH("Fs1: Failed to read from file: %s (%d)", Status, RtlGetStatusString(Status));
	
	LogMsg("Read in %lld bytes.", Iosb.BytesRead);
	if (Iosb.BytesRead > SizePrint)
		Iosb.BytesRead = SizePrint;
	DumpHex(Buffer, Iosb.BytesRead);
	
	LogMsg("Closing now.");
	OSClose(Handle);
	OSFree(Buffer);
}

void RunTest1()
{
	FileList("/Mount/Nvme0Disk1Part0/ns/initrd/Game/celeste.nes", 0xCB30, 4096, 512);
}
