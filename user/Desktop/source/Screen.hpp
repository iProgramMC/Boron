//
//  Screen.hpp
//
//  Copyright (C) 2026 iProgramInCpp.
//  Created on 9/4/2026.
//
#pragma once

#include <boron.h>
#include <cg/context.h>

#include "File.hpp"

class Screen
{
public:
	Screen(HANDLE FramebufferHandle);
	~Screen();
	
	BSTATUS GetCreateStatus() const {
		return m_CreateStatus;
	}

private:
	File m_FramebufferFile;
	PGRAPHICS_CONTEXT m_Context;
	BSTATUS m_CreateStatus = STATUS_SUCCESS;
};
