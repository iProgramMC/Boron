// 10 November 2025 - iProgramInCpp
#include <boron.h>

int _start()
{
	PPEB Peb = OSGetCurrentPeb();
	while (true)
	{
		OSPrintf("Type something in: ");
		
		char Buffer[500];
		IO_STATUS_BLOCK Iosb;
		BSTATUS Status = OSReadFile(&Iosb, Peb->StandardInput, 0, Buffer, sizeof Buffer - 1, IO_RW_FINISH_ON_NEWLINE);
		if (FAILED(Status))
		{
			OSPrintf("FAILED to read: %s\n", RtlGetStatusString(Status));
			OSSleep(2000);
		}
		
		Buffer[sizeof Buffer - 1] = 0;
		Buffer[Iosb.BytesRead] = 0;
		OSPrintf("You typed in: '%s'.\n", Buffer);
	}
}
