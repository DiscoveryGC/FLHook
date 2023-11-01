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
#include "PlayerRestrictions.h"

//#include "./headers/FLCoreServer.h"
//#include "./headers/FLCoreCommon.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Structures and shit yo
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

list<wstring> angels;
list<wstring> events;
list<uint> racestartids;
map<uint, uint> bastillebase;

map<uint, byte> racecountdown;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Settings Loading
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void AP::LoadSettings()
{
	// The path to the configuration file.
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\alley_permissions.cfg";

	INI_Reader ini;
	if (ini.open(scPluginCfgFile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("angels"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("permission"))
					{
						angels.push_back(stows(ini.get_value_string()));
					}
				}
			}
			else if (ini.is_header("event"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("permission"))
					{
						events.push_back(stows(ini.get_value_string()));
					}
				}
			}
			else if (ini.is_header("racestart"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("id"))
					{
						racestartids.push_back(CreateID(ini.get_value_string()));
					}
				}
			}
			else if (ini.is_header("bastillebase"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("base"))
					{
						uint base = CreateID(ini.get_value_string());
						bastillebase[base] = base;
					}
				}
			}
		}
		ini.close();
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Dependencies
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct DEFERREDJUMPS
{
	uint system;
	Vector pos;
	Matrix ornt;
};
static map<uint, DEFERREDJUMPS> mapDeferredJumps;

void SwitchSystem(uint iClientID, uint system, Vector pos, Matrix ornt)
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
		VirtualProtect(SwitchOut + 0xd7, 200, PAGE_EXECUTE_READWRITE, &dummy);
	}

	// Patch the system switch out routine to put the ship in a
	// system of our choosing.
	if (mapDeferredJumps.find(iClientID) != mapDeferredJumps.end())
	{
		uint iSystemID = mapDeferredJumps[iClientID].system;
		SwitchOut[0x0d7] = 0xeb;				// ignore exit object
		SwitchOut[0x0d8] = 0x40;
		SwitchOut[0x119] = 0xbb;				// set the destination system
		*(PDWORD)(SwitchOut + 0x11a) = iSystemID;
		SwitchOut[0x266] = 0x45;				// don't generate warning
		*(float*)(SwitchOut + 0x2b0) = mapDeferredJumps[iClientID].pos.z;		// set entry location
		*(float*)(SwitchOut + 0x2b8) = mapDeferredJumps[iClientID].pos.y;
		*(float*)(SwitchOut + 0x2c0) = mapDeferredJumps[iClientID].pos.x;
		*(float*)(SwitchOut + 0x2c8) = mapDeferredJumps[iClientID].ornt.data[2][2];
		*(float*)(SwitchOut + 0x2d0) = mapDeferredJumps[iClientID].ornt.data[1][1];
		*(float*)(SwitchOut + 0x2d8) = mapDeferredJumps[iClientID].ornt.data[0][0];
		*(float*)(SwitchOut + 0x2e0) = mapDeferredJumps[iClientID].ornt.data[2][1];
		*(float*)(SwitchOut + 0x2e8) = mapDeferredJumps[iClientID].ornt.data[2][0];
		*(float*)(SwitchOut + 0x2f0) = mapDeferredJumps[iClientID].ornt.data[1][2];
		*(float*)(SwitchOut + 0x2f8) = mapDeferredJumps[iClientID].ornt.data[1][0];
		*(float*)(SwitchOut + 0x300) = mapDeferredJumps[iClientID].ornt.data[0][2];
		*(float*)(SwitchOut + 0x308) = mapDeferredJumps[iClientID].ornt.data[0][1];
		*(PDWORD)(SwitchOut + 0x388) = 0x03ebc031;		// ignore entry object
		mapDeferredJumps.erase(iClientID);
		pub::SpaceObj::SetInvincible(iShip, false, false, 0);
		Server.SystemSwitchOutComplete(iShip, iClientID);
		SwitchOut[0x0d7] = 0x0f;
		SwitchOut[0x0d8] = 0x84;
		SwitchOut[0x119] = 0x87;
		*(PDWORD)(SwitchOut + 0x11a) = 0x1b8;
		*(PDWORD)(SwitchOut + 0x25d) = 0x1cf7f;
		SwitchOut[0x266] = 0x1a;
		*(float*)(SwitchOut + 0x2b0) =
			*(float*)(SwitchOut + 0x2b8) =
			*(float*)(SwitchOut + 0x2c0) = 0;
		*(float*)(SwitchOut + 0x2c8) =
			*(float*)(SwitchOut + 0x2d0) =
			*(float*)(SwitchOut + 0x2d8) = 1;
		*(float*)(SwitchOut + 0x2e0) =
			*(float*)(SwitchOut + 0x2e8) =
			*(float*)(SwitchOut + 0x2f0) =
			*(float*)(SwitchOut + 0x2f8) =
			*(float*)(SwitchOut + 0x300) =
			*(float*)(SwitchOut + 0x308) = 0;
		*(PDWORD)(SwitchOut + 0x388) = 0xcf8b178b;

		CUSTOM_JUMP_STRUCT info;
		info.iShipID = iShip;
		info.iSystemID = iSystemID;
		Plugin_Communication(CUSTOM_JUMP, &info);

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

/* Check if the user is in bastille */

void AP::BaseEnter_AFTER(uint base, uint iClientID)
{
	//PrintUserCmdText(iClientID, L"Base ID: %08x", base);

	// is it a base of terror
	if (bastillebase.find(base) != bastillebase.end())
	{
		PrintUserCmdText(iClientID, L"You are in the prison system for violating the server rules.");
		PrintUserCmdText(iClientID, L"Head to the Sanction Notices section of the forums for more information.");
		PrintUserCmdText(iClientID, L"If there is no sanction for this ship, contact the administration team on the forums.");
		PrintUserCmdText(iClientID, L"Forums: http://www.discoverygc.com/forums/");
	}
}

bool AP::AlleyCmd_Chase(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
	wstring rights;

	bool isAngel = false;

	// is it an admin		
	if (HkGetAdmin((const wchar_t*)Players.GetActiveCharacterName(iClientID), rights) == HKE_OK)
	{
		//PrintUserCmdText(iClientID, L"You are a platinum level Admin. Use $help for a list of available commands.");
		isAngel = true;
	}

	// is it an angel
	list<wstring>::iterator iter = angels.begin();
	while (iter != angels.end())
	{
		if (*iter == wscCharname)
			isAngel = true;
		iter++;
	}

	if (isAngel == false)
	{
		PrintUserCmdText(iClientID, L"You are not an Angel. You will now be permabanned.");
		return true;
	}

	wstring wscTargetCharname = GetParam(wscParam, ' ', 0);
	if (!wscTargetCharname.length())
	{
		PrintUserCmdText(iClientID, L"ERR Invalid parameters");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	HKPLAYERINFO adminPlyr;
	if (HkGetPlayerInfo(wscCharname, adminPlyr, false) != HKE_OK)
	{
		PrintUserCmdText(iClientID, L"ERR Not in space\n");
		return true;
	}



	HKPLAYERINFO targetPlyr;
	if (HkGetPlayerInfo(wscTargetCharname, targetPlyr, false) != HKE_OK || targetPlyr.iShip == 0)
	{
		PrintUserCmdText(iClientID, L"ERR Player not found or not in space\n");
		return true;
	}

	Vector pos;
	Matrix ornt;
	pub::SpaceObj::GetLocation(targetPlyr.iShip, pos, ornt);
	pos.y += 100;

	PrintUserCmdText(iClientID, L"Jump to system=%s x=%0.0f y=%0.0f z=%0.0f\n", targetPlyr.wscSystem.c_str(), pos.x, pos.y, pos.z);
	SwitchSystem(adminPlyr.iClientID, targetPlyr.iSystem, pos, ornt);

	return true;
}


/* When a chat message is sent we check if an angel player is trying to use an angel command
	bool AP::HkCb_SendChat(uint iClientID, uint iTo, uint iSize, void *rdlReader)
	{
		wstring wscCharname = (const wchar_t*) Players.GetActiveCharacterName(iClientID);

		list<wstring>::iterator iter = angels.find(wscCharname);
		if (iter!=angels.end())
		{
			PrintUserCmdText(iClientID, L"You are a platinum level Angel.");

			// Extract text from rdlReader
			BinaryRDLReader rdl;
			wchar_t wszBuf[1024];
			uint iRet1;
			rdl.extract_text_from_buffer((unsigned short*)wszBuf, sizeof(wszBuf), iRet1, (const char*)rdlReader, iSize);
			wstring wscChatMsg = wszBuf;

			// Find the ': ' which indicates the end of the sending player name.
			size_t iTextStartPos = wscChatMsg.find(L": ");
			if (iTextStartPos != string::npos)
			{
				if ((wscChatMsg.find(L": /")==iTextStartPos && wscChatMsg.find(L": //")!=iTextStartPos)
					|| wscChatMsg.find(L": .")==iTextStartPos)
				{
					return true;
				}
			}
		}
		return false;
	}

*/

bool AP::RacestartCmd(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	uint iShip = 0;
	pub::Player::GetShip(iClientID, iShip);
	if (!iShip) {
		PrintUserCmdText(iClientID, L"Error: You are docked");
		return true;
	}

	bool isAllowed = false;

	for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
	{
		if (find(racestartids.begin(), racestartids.end(), item->iArchID) != racestartids.end() && item->bMounted)
		{
			isAllowed = true;
			break;
		}
	}

	if (isAllowed)
	{
		uint iShip;
		pub::Player::GetShip(iClientID, iShip);

		uint iSystem;
		pub::Player::GetSystem(iClientID, iSystem);

		struct PlayerData *pPD = 0;
		while (pPD = Players.traverse_active(pPD))
		{
			uint iClientID2 = HkGetClientIdFromPD(pPD);
			uint iSystem2 = 0;
			pub::Player::GetSystem(iClientID2, iSystem2);
			if (iSystem != iSystem2)
				continue;

			uint iShip2;
			pub::Player::GetShip(iClientID2, iShip2);
			if (HkDistance3DByShip(iShip, iShip2) < 5000.0f)
			{
				racecountdown[iClientID2] = 7;
			}
		}

		PrintUserCmdText(iClientID, L"Sending race start audio and mission objectives to everyone within 5K");
	}
	else
	{
		PrintUserCmdText(iClientID, L"You are not allowed to use this.");
	}

	return true;
}

void ShowPlayerMissionText(uint iClientID, const wstring &text)
{
	HkChangeIDSString(iClientID, 526999, text);

	FmtStr caption(0, 0);
	caption.begin_mad_lib(526999);
	caption.end_mad_lib();

	pub::Player::DisplayMissionMessage(iClientID, caption, MissionMessageType::MissionMessageType_Type2, true);
}

void AP::Timer()
{
	// every second, decrement race countdowns
	for (map<uint, byte>::iterator i = racecountdown.begin(); i != racecountdown.end(); ++i)
	{
		if (i->second == 7)
		{
			pub::Audio::PlaySoundEffect(i->first, CreateID("dsy_racestart"));
		}
		else if (i->second == 5)
		{
			ShowPlayerMissionText(i->first, L"Countdown: 3");
		}
		else if (i->second == 3)
		{
			ShowPlayerMissionText(i->first, L"Countdown: 2");
		}
		else if (i->second == 2)
		{
			ShowPlayerMissionText(i->first, L"Countdown: 1");
		}
		else if (i->second == 0)
		{
			ShowPlayerMissionText(i->first, L"Go!!!");
			racecountdown.erase(i->first);
		}
		i->second--;
	}
}