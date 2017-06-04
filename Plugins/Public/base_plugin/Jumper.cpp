// AlleyPlugin for FLHookPlugin
// January 2015 by Alley
//
// 
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Includes
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <math.h>
#include <float.h>
#include <FLHook.h>
#include <plugin.h>
#include <list>
#include <set>

#include <PluginUtilities.h>
#include "Main.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Structures and shit yo
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct DEFERREDJUMPS
	{
		uint system;
		Vector pos;
		Matrix ornt;
	};
	static map<uint, DEFERREDJUMPS> mapDeferredJumps;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Settings Loading
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Dependencies
///////////////////////////////////////////////////////////////////////////////////////////////////////////////



	void AP::SwitchSystem(uint iClientID, uint system, Vector pos, Matrix ornt)
	{
		mapDeferredJumps[iClientID].system = system;
		mapDeferredJumps[iClientID].pos = pos;
		mapDeferredJumps[iClientID].ornt = ornt;

		// Force a launch to put the ship in the right location in the current system so that
		// when the change system command arrives (hopefully) a fraction of a second later
		// the ship will appear at the right location.
		HkRelocateClient(iClientID, pos, ornt);
		// Send the jump command to the client. The client will send a system switch out complete
		// event which we intercept to set the new starting positions.
		PrintUserCmdText(iClientID, L" ChangeSys %u", system);
	}

	bool AP::SystemSwitchOutComplete(unsigned int iShip, unsigned int iClientID)
	{
		static PBYTE SwitchOut = 0;
		if (!SwitchOut)
		{
			SwitchOut = (PBYTE)hModServer + 0xf600;

			DWORD dummy;
			VirtualProtect(SwitchOut+0xd7, 200, PAGE_EXECUTE_READWRITE, &dummy);
		}

		// Patch the system switch out routine to put the ship in a
		// system of our choosing.
		if (mapDeferredJumps.find(iClientID) != mapDeferredJumps.end())
		{
			SwitchOut[0x0d7] = 0xeb;				// ignore exit object
			SwitchOut[0x0d8] = 0x40;
			SwitchOut[0x119] = 0xbb;				// set the destination system
			*(PDWORD)(SwitchOut+0x11a) = mapDeferredJumps[iClientID].system;
			SwitchOut[0x266] = 0x45;				// don't generate warning
			*(float*)(SwitchOut+0x2b0) = mapDeferredJumps[iClientID].pos.z;		// set entry location
			*(float*)(SwitchOut+0x2b8) = mapDeferredJumps[iClientID].pos.y;
			*(float*)(SwitchOut+0x2c0) = mapDeferredJumps[iClientID].pos.x;
			*(float*)(SwitchOut+0x2c8) = mapDeferredJumps[iClientID].ornt.data[2][2];
			*(float*)(SwitchOut+0x2d0) = mapDeferredJumps[iClientID].ornt.data[1][1];
			*(float*)(SwitchOut+0x2d8) = mapDeferredJumps[iClientID].ornt.data[0][0];
			*(float*)(SwitchOut+0x2e0) = mapDeferredJumps[iClientID].ornt.data[2][1];
			*(float*)(SwitchOut+0x2e8) = mapDeferredJumps[iClientID].ornt.data[2][0];
			*(float*)(SwitchOut+0x2f0) = mapDeferredJumps[iClientID].ornt.data[1][2];
			*(float*)(SwitchOut+0x2f8) = mapDeferredJumps[iClientID].ornt.data[1][0];
			*(float*)(SwitchOut+0x300) = mapDeferredJumps[iClientID].ornt.data[0][2];
			*(float*)(SwitchOut+0x308) = mapDeferredJumps[iClientID].ornt.data[0][1];
			*(PDWORD)(SwitchOut+0x388) = 0x03ebc031;		// ignore entry object
			mapDeferredJumps.erase(iClientID);
			pub::SpaceObj::SetInvincible(iShip, false, false, 0);
			Server.SystemSwitchOutComplete(iShip,iClientID);
			SwitchOut[0x0d7] = 0x0f;
			SwitchOut[0x0d8] = 0x84;
			SwitchOut[0x119] = 0x87;
			*(PDWORD)(SwitchOut+0x11a) = 0x1b8;
			*(PDWORD)(SwitchOut+0x25d) = 0x1cf7f;
			SwitchOut[0x266] = 0x1a;
			*(float*)(SwitchOut+0x2b0) =
				*(float*)(SwitchOut+0x2b8) =
				*(float*)(SwitchOut+0x2c0) = 0;
			*(float*)(SwitchOut+0x2c8) =
				*(float*)(SwitchOut+0x2d0) =
				*(float*)(SwitchOut+0x2d8) = 1;
			*(float*)(SwitchOut+0x2e0) =
				*(float*)(SwitchOut+0x2e8) =
				*(float*)(SwitchOut+0x2f0) =
				*(float*)(SwitchOut+0x2f8) =
				*(float*)(SwitchOut+0x300) =
				*(float*)(SwitchOut+0x308) = 0;
			*(PDWORD)(SwitchOut+0x388) = 0xcf8b178b;

			return true;
		}
		return false;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Logic
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
void AP::ClearClientInfo(uint iClientID)
	{
		mapDeferredJumps.erase(iClientID);
	}
