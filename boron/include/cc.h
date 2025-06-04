/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	cc.h
	
Abstract:
	This header defines all of the structures necessary
	for the implementation of the Boron view cache manager.
	
Author:
	iProgramInCpp - 23 May 2025
***/
#pragma once

typedef struct _MMVAD_ENTRY MMVAD, *PMMVAD;
typedef struct _FCB FCB, *PFCB;

// -- View Cache LRU List --

// Adds a VAD to the view cache LRU list.
void CcAddVadToViewCacheLru(PMMVAD Vad);

// Removes a VAD from the view cache LRU list.
void CcRemoveVadFromViewCacheLru(PMMVAD Vad);

// Moves a VAD to the front of the view cache LRU list.
// This is called when a system space VAD is faulted on.
void CcOnSystemSpaceVadUsed(PMMVAD Vad);

// Removes the head of the view cache LRU list for freeing.
// This is used if view space is running out.
PMMVAD CcRemoveHeadOfViewCacheLru();

// Purge any VADs that go over the limit plus a specified number.
void CcPurgeViewsOverLimit(int LeaveSpaceFor);

// Purge any VADs associated with this FCB.
// This function is to be called when the FCB loses its last reference.
void CcPurgeViewsForFile(PFCB Fcb);
