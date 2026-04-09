//
//  Screen.cpp
//
//  Copyright (C) 2026 iProgramInCpp.
//  Created on 9/4/2026.
//
#include "Screen.hpp"

Screen::Screen(HANDLE framebufferHandle) : m_handle(framebufferHandle)
{
	
}

Screen::~Screen()
{
	CGFreeContext(m_context);
}


