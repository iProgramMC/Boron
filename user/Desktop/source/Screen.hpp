//
//  Screen.hpp
//
//  Copyright (C) 2026 iProgramInCpp.
//  Created on 9/4/2026.
//
#pragma once

#include <boron.h>
#include <cg/context.h>

class Screen
{
public:
	Screen(HANDLE framebufferHandle);
	~Screen();

private:
	HANDLE m_handle;
	PGRAPHICS_CONTEXT m_context;
};
