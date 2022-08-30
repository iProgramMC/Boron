// Terminal driver
#define STB_SPRINTF_IMPLEMENTATION // implement the stb_sprintf right here
#include <main.h>
#include <hal.h>
#include <string.h>

void LogMsg(const char* msg, ...)
{
	va_list va;
	va_start(va, msg);
	char buffer[8192]; // should be big enough...
	buffer[sizeof buffer - 3] = 0;
	int chars = vsnprintf(buffer, sizeof buffer - 3, msg, va);
	strcpy(buffer + chars, "\n");
	va_end(va);
	
	// this one goes to the screen
	HalPrintString(buffer);
}

void SLogMsg(const char* msg, ...)
{
	va_list va;
	va_start(va, msg);
	char buffer[8192]; // should be big enough...
	buffer[sizeof buffer - 1] = 0;
	int chars = vsnprintf(buffer, sizeof buffer - 1, msg, va);
	strcpy(buffer + chars, "\n");
	va_end(va);
	
	// this one goes to the screen
	HalPrintStringDebug(buffer);
}
