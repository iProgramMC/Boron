//
//  crt0.c
//  Copyright (C) 2026 iProgramInCpp
//  Created on 9/4/2026.
//
//  This implements the entry point of all applications, as well as small
//  pieces of functionality required for C++ support.

// NOTE: This must *all* be in 1 file!
void* __dso_handle;

extern int main(int ArgumentCount, char** ArgumentValues, char** Environment);

int _start(int ArgumentCount, char** ArgumentValues, char** Environment)
{
	// note: at this point, we're still wrapped by OSDLL (hopefully),
	// so if we return from here it'll just return there and call the destructors.
	return main(ArgumentCount, ArgumentValues, Environment);
}
