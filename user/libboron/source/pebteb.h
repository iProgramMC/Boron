#pragma once

#include <pss.h>
#include <eb.h>

// Creates a TEB but does not assign it to any thread.
PTEB OSDLLCreateTebObject(PPEB Peb);

// Creates a TEB for the current thread.
BSTATUS OSDLLCreateTeb(PPEB Peb);
