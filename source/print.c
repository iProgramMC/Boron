// Terminal driver
#define STB_SPRINTF_IMPLEMENTATION // implement the stb_sprintf right here
#include <ke.h>
#include <hal.h>
#include <string.h>

// TODO: Have a worker thread or service?
SpinLock g_PrintLock;
SpinLock g_DebugPrintLock;

void LogMsg(const char* msg, ...)
{
	KeLock(&g_PrintLock);
	va_list va;
	va_start(va, msg);
	char buffer[512]; // should be big enough...
	buffer[sizeof buffer - 3] = 0;
	int chars = vsnprintf(buffer, sizeof buffer - 3, msg, va);
	strcpy(buffer + chars, "\n");
	va_end(va);
	
	// this one goes to the screen
	HalPrintString(buffer);
	KeUnlock(&g_PrintLock);
}

void SLogMsg(const char* msg, ...)
{
	KeLock(&g_DebugPrintLock);
	va_list va;
	va_start(va, msg);
	char buffer[512]; // should be big enough...
	buffer[sizeof buffer - 1] = 0;
	int chars = vsnprintf(buffer, sizeof buffer - 1, msg, va);
	strcpy(buffer + chars, "\n");
	va_end(va);
	
	// this one goes to the screen
	HalPrintStringDebug(buffer);
	KeUnlock(&g_DebugPrintLock);
}
