#include <boron.h>
#include <string.h>
#include "misc.h"

void RunTest6()
{
	const char* Path = "/Devices/FrameBuffer0";
	
	OBJECT_ATTRIBUTES Attributes;
	Attributes.ObjectName = Path;
	Attributes.ObjectNameLength = strlen(Path);
	Attributes.OpenFlags = 0;
	Attributes.RootDirectory = HANDLE_NONE;
	
	HANDLE Handle;
	BSTATUS Status = OSOpenFile(&Handle, &Attributes);
	if (FAILED(Status))
	{
		DbgPrint("Test6: Failed to open %s. %s", Path, RtlGetStatusString(Status));
		return;
	}
	
	IOCTL_FRAMEBUFFER_INFO Info;
	Status = OSDeviceIoControl(
		Handle,
		IOCTL_FRAMEBUFFER_GET_INFO,
		NULL,
		0,
		&Info,
		sizeof Info
	);
	if (FAILED(Status))
	{
		DbgPrint("Test6: Failed to get framebuffer info from %s. %s", Path, RtlGetStatusString(Status));
		OSClose(Handle);
		return;
	}
	
	void* BaseAddress = NULL;
	size_t Size = Info.Pitch * Info.Height;
	Status = OSMapViewOfObject(
		CURRENT_PROCESS_HANDLE,
		Handle,
		&BaseAddress,
		Size,
		MEM_COMMIT,
		0,
		PAGE_READ | PAGE_WRITE
	);
	
	if (FAILED(Status))
	{
		DbgPrint("Test6: Failed to map %s. %s", Path, RtlGetStatusString(Status));
		OSClose(Handle);
		return;
	}

	// Draw a recognizable pattern
	uint32_t RowStart = 0;
	
	uint32_t Thr[7];
	for (int i = 0; i < 7; i++) {
		Thr[i] = Info.Width * (i + 1) / 7;
	}
	uint32_t Thr2[6];
	for (int i = 0; i < 6; i++) {
		Thr2[i] = Info.Width * (i + 1) / 6;
	}

	uint32_t ThrV = Info.Height * 2 / 3;
	uint32_t ThrV2 = Info.Height * 3 / 4;
	
	const uint32_t BarColors1[] = { 0xc0c0c0, 0xc0c000, 0x00c0c0, 0x00c000, 0xc000c0, 0xc00000, 0x0000c0 };
	const uint32_t BarColors2[] = { 0x0000c0, 0x131313, 0xc000c0, 0x131313, 0x00c0c0, 0x131313, 0xc0c0c0 };
	const uint32_t BarColors3[] = { 0x00214c, 0xffffff, 0x32006a, 0x131313, 0xffffff, 0x131313 };
	const uint32_t BarColors4[] = { 0x090909, 0x131313, 0x1d1d1d };
	
	for (uint32_t i = 0; i < Info.Height; i++)
	{
		uint32_t* Row = (uint32_t*)((uint8_t*)BaseAddress + RowStart);
		
		for (uint32_t j = 0; j < Info.Width; j++)
		{
			uint32_t Color = 0xFFFFFF;
			
			if (i < ThrV)
			{
				for (int k = 0; k < 7; k++) {
					if (j < Thr[k]) {
						Color = BarColors1[k];
						break;
					}
				}
			}
			else if (i < ThrV2)
			{
				for (int k = 0; k < 7; k++) {
					if (j < Thr[k]) {
						Color = BarColors2[k];
						break;
					}
				}
			}
			else {
				for (int k = 0; k < 6; k++) {
					if (j < Thr2[k]) {
						Color = BarColors3[k];
						if (k == 4) {
							uint32_t diff1 = j - Thr2[k - 1];
							uint32_t diff2 = Thr2[k] - Thr2[k - 1];
							
							if (diff1 < diff2 * 1 / 3)
								Color = BarColors4[0];
							else if (diff1 < diff2 * 2 / 3)
								Color = BarColors4[1];
							else
								Color = BarColors4[2];
						}
						break;
					}
				}
			}
			
			Row[j] = Color;
		}
		
		RowStart += Info.Pitch;
	}
	
	Status = OSFreeVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		BaseAddress,
		Size,
		MEM_RELEASE
	);
	
	if (FAILED(Status))
	{
		DbgPrint("Test6: Failed to unmap %s. %s", Path, RtlGetStatusString(Status));
		OSClose(Handle);
		return;
	}
	
	OSClose(Handle);
}