#include <boron.h>

int _start()
{
	PPEB Peb = OSGetCurrentPeb();
	while (true)
	{
		OSPrintf("Type something in: ");
		
		char Buffer[512];
		IO_STATUS_BLOCK Iosb;
		BSTATUS Status = OSReadFile(&Iosb, Peb->StandardInput, 0, Buffer, sizeof Buffer, IO_RW_FINISH_ON_NEWLINE);
		if (FAILED(Status))
		{
			OSPrintf("FAILED to read: %s\n", RtlGetStatusString(Status));
			OSSleep(2000);
		}
		
		OSPrintf("You typed in: '%s'.\n", Buffer);
	}
}
