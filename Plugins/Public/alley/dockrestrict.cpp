// AlleyPlugin for FLHookPlugin
// February 2015 by Alley
//
// This CPP controls the function to prevent players from being able to dock to anything for x seconds.
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
#include <unordered_set>
#include <sstream>
#include <iostream>

#include <PluginUtilities.h>
#include "PlayerRestrictions.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Structures and shit yo
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

static list<uint> idlist;
static list<uint> listAllowedShips;
static map<uint, wstring> MapActiveSirens;

static int duration = 60;
static map<uint, int> mapActiveNoDock;

static unordered_set<uint> baseblacklist;

static list<wstring> superNoDockedShips;

FILE *Logfile = fopen("./flhook_logs/nodockcommand.log", "at");

void Logging(const char *szString, ...)
{
	char szBufString[1024];
	va_list marker;
	va_start(marker, szString);
	_vsnprintf(szBufString, sizeof(szBufString) - 1, szString, marker);

	if (Logfile) {
		char szBuf[64];
		time_t tNow = time(0);
		struct tm *t = localtime(&tNow);
		strftime(szBuf, sizeof(szBuf), "%d/%m/%Y %H:%M:%S", t);
		fprintf(Logfile, "%s %s\n", szBuf, szBufString);
		fflush(Logfile);
		fclose(Logfile);
	}
	else {
		ConPrint(L"Failed to write nodockcommand log! This might be due to inability to create the directory - are you running as an administrator?\n");
	}
	Logfile = fopen("./flhook_logs/nodockcommand.log", "at");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Settings Loading
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ADOCK::LoadSettings()
{
	// The path to the configuration file.
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\alley.cfg";

	INI_Reader ini;
	if (ini.open(scPluginCfgFile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("nodockcommand"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("id"))
					{
						idlist.push_back(CreateID(ini.get_value_string(0)));
					}
				}
			}
			else if (ini.is_header("nodockexemption"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("no"))
					{
						baseblacklist.insert(CreateID(ini.get_value_string(0)));
					}
				}
			}
			else if (ini.is_header("config"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("duration"))
					{
						duration = ini.get_value_int(0);
					}
				}
			}
			else if (ini.is_header("supernodockedships"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("ship"))
					{
						superNoDockedShips.push_back((const wchar_t*)ini.get_value_wstring());
					}
				}
			}
		}
		ini.close();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Logic
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ADOCK::Timer()
{
	for (map<uint, int>::iterator i = mapActiveNoDock.begin(); i != mapActiveNoDock.end(); ++i)
	{
		if (i->second == 0)
		{
			const wchar_t* wszCharname = (const wchar_t*)Players.GetActiveCharacterName(i->first);
			if (wszCharname) {
				wstring wscMsg = L"%time %victim's docking rights have been restored.";
				wscMsg = ReplaceStr(wscMsg, L"%time", GetTimeString(false));
				wscMsg = ReplaceStr(wscMsg, L"%victim", wszCharname);
				PrintLocalUserCmdText(i->first, wscMsg, 10000);
			}

			mapActiveNoDock.erase(i->first);
		}
		else
		{
			i->second = i->second - 1;
		}
	}
}

void ADOCK::ClearClientInfo(uint iClientID)
{
	listAllowedShips.remove(iClientID);
	MapActiveSirens.erase(iClientID);
}

void ADOCK::PlayerLaunch(unsigned int iShip, unsigned int client)
{
	// Retrieve the location and cargo list.
	int iHoldSize;
	list<CARGO_INFO> lstCargo;
	HkEnumCargo((const wchar_t*)Players.GetActiveCharacterName(client), lstCargo, iHoldSize);

	foreach(lstCargo, CARGO_INFO, i)
	{
		if (i->bMounted)
		{
			// is it a good id
			list<uint>::iterator iter = idlist.begin();
			while (iter != idlist.end())
			{
				if (*iter == i->iArchID)
				{
					listAllowedShips.push_back(client);
					//PrintUserCmdText(client, L"DEBUG: Cat = yes");
					break;
				}
				iter++;
			}
		}
	}

	//PrintUserCmdText(client, L"DEBUG: Cat = no");
	//PrintUserCmdText(client, L"DEBUG: We have %d skittles", duration);
}

bool ADOCK::NoDockCommand(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
	bool isAllowed = false;

	list<uint>::iterator iter = listAllowedShips.begin();
	while (iter != listAllowedShips.end())
	{
		if (*iter == iClientID)
		{
			isAllowed = true;
			break;
		}
		iter++;
	}

	if (isAllowed == false)
	{
		PrintUserCmdText(iClientID, L"You are not allowed to use this.");
		return true;
	}
	if (isAllowed == true)
	{
		PrintUserCmdText(iClientID, L"You are allowed to use this.");

		uint iShip = 0;
		pub::Player::GetShip(iClientID, iShip);
		if (!iShip) {
			PrintUserCmdText(iClientID, L"Error: You are docked");
			return true;
		}

		uint iTarget = 0;
		pub::SpaceObj::GetTarget(iShip, iTarget);

		if (!iTarget) {
			PrintUserCmdText(iClientID, L"Error: No target");
			return true;
		}

		uint iClientIDTarget = HkGetClientIDByShip(iTarget);
		if (!HkIsValidClientID(iClientIDTarget))
		{
			PrintUserCmdText(iClientID, L"Error: Target is no player");
			return true;
		}

		wstring wscTargetCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientIDTarget);

		for (map<uint, int>::iterator i = mapActiveNoDock.begin(); i != mapActiveNoDock.end(); ++i)
		{
			if (i->first == iClientIDTarget)
			{
				PrintUserCmdText(iClientID, L"OK Removal of docking rights reset to %d seconds", duration);
				PrintUserCmdText(iClientIDTarget, L"Removal of docking rights reset to %d seconds", duration);
				i->second = duration;
				return true;
			}
		}

		mapActiveNoDock[iClientIDTarget] = duration;

		//10k space message

		stringstream ss;
		ss << duration;
		string strduration = ss.str();

		wstring wscMsg = L"%time %victim's docking rights have been removed by %player for minimum %duration seconds";
		wscMsg = ReplaceStr(wscMsg, L"%time", GetTimeString(false));
		wscMsg = ReplaceStr(wscMsg, L"%player", (const wchar_t*)Players.GetActiveCharacterName(iClientID));
		wscMsg = ReplaceStr(wscMsg, L"%victim", wscTargetCharname.c_str());
		wscMsg = ReplaceStr(wscMsg, L"%duration", stows(strduration).c_str());
		PrintLocalUserCmdText(iClientID, wscMsg, 10000);

		//internal log
		wstring wscMsgLog = L"<%sender> removed docking rights from <%victim>";
		wscMsgLog = ReplaceStr(wscMsgLog, L"%sender", wscCharname.c_str());
		wscMsgLog = ReplaceStr(wscMsgLog, L"%victim", wscTargetCharname.c_str());
		string scText = wstos(wscMsgLog);
		Logging("%s", scText.c_str());

		return true;
	}

	return true;
}

bool ADOCK::IsDockAllowed(uint iShip, uint iDockTarget, uint iClientID)
{
	if (!superNoDockedShips.empty())
	{
		boolean supernodocked = false;
		wstring curCharName = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
		list<wstring>::iterator sndIter = superNoDockedShips.begin();
		while (sndIter != superNoDockedShips.end())
		{
			if (*sndIter == curCharName)
			{
				supernodocked = true;
				break;
			}
			sndIter++;
		}

		if (supernodocked)
		{
			uint iID;
			pub::SpaceObj::GetDockingTarget(iDockTarget, iID);
			Universe::IBase* base = Universe::get_base(iID);
			if (base)
			{
				PrintUserCmdText(iClientID, L"You are not allowed to dock on any base.");
				return false;
			}
		}
	}

	// instead of complicated code, we just check if he's nice. If so, we ignore the rest of the code.
	if (!mapActiveNoDock.count(iClientID))
	{
		return true;
	}

	uint base;
	pub::SpaceObj::GetDockingTarget(iDockTarget, base);

	// if he's not nice, we check if the base is subject to nodock effect.
	if (baseblacklist.count(base))
	{
		//we have found this base in the blacklist. nodock will therefore work. don't let him dock.
		pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("info_access_denied"));
		PrintUserCmdText(iClientID, L"You are currently not allowed to dock on this base.");
		return false;
	}

	//otherwise this is probably not meant to work	
	return true;
}

void ADOCK::AdminNoDock(CCmds* cmds, const wstring &wscCharname)
{
	if (cmds->rights != RIGHT_SUPERADMIN)
	{
		cmds->Print(L"ERR No permission\n");
		return;
	}

	HKPLAYERINFO targetPlyr;
	if (HkGetPlayerInfo(wscCharname, targetPlyr, false) != HKE_OK)
	{
		cmds->Print(L"ERR Player not found\n");
		return;
	}

	for (map<uint, int>::iterator i = mapActiveNoDock.begin(); i != mapActiveNoDock.end(); ++i)
	{
		if (i->first == targetPlyr.iClientID)
		{
			cmds->Print(L"OK Removal of docking rights reset to %d seconds", duration);
			PrintUserCmdText(targetPlyr.iClientID, L"Removal of docking rights reset to %d seconds", duration);
			i->second = duration;
			return;
		}
	}

	mapActiveNoDock[targetPlyr.iClientID] = duration;

	//10k space message

	stringstream ss;
	ss << duration;
	string strduration = ss.str();

	wstring wscMsg = L"%time %victim's docking rights have been removed by %player for minimum %duration seconds";
	wscMsg = ReplaceStr(wscMsg, L"%time", GetTimeString(false));
	wscMsg = ReplaceStr(wscMsg, L"%player", cmds->GetAdminName().c_str());
	wscMsg = ReplaceStr(wscMsg, L"%victim", targetPlyr.wscCharname.c_str());
	wscMsg = ReplaceStr(wscMsg, L"%duration", stows(strduration).c_str());
	PrintLocalUserCmdText(targetPlyr.iClientID, wscMsg, 10000);

	//internal log
	wstring wscMsgLog = L"<%sender> removed docking rights from <%victim>";
	wscMsgLog = ReplaceStr(wscMsgLog, L"%sender", cmds->GetAdminName().c_str());
	wscMsgLog = ReplaceStr(wscMsgLog, L"%victim", targetPlyr.wscCharname.c_str());
	string scText = wstos(wscMsgLog);
	Logging("%s", scText.c_str());

	cmds->Print(L"OK\n");
	return;
}

bool ADOCK::PoliceCmd(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	uint iShip = 0;
	pub::Player::GetShip(iClientID, iShip);
	if (!iShip) {
		PrintUserCmdText(iClientID, L"Error: You are docked");
		return true;
	}

	bool isAllowed = false;

	list<uint>::iterator iter = listAllowedShips.begin();
	while (iter != listAllowedShips.end())
	{
		if (*iter == iClientID)
		{
			isAllowed = true;
			break;
		}
		iter++;
	}

	if (isAllowed == false)
	{
		PrintUserCmdText(iClientID, L"You are not allowed to use this.");
		return true;
	}
	if (isAllowed == true)
	{
		if (MapActiveSirens.find(iClientID) != MapActiveSirens.end())
		{
			UnSetFuse(iClientID, CreateID("dsy_police_liberty"));
			MapActiveSirens.erase(iClientID);
			PrintUserCmdText(iClientID, L"Police system deactivated.");
		}
		else
		{
			SetFuse(iClientID, CreateID("dsy_police_liberty"), 999999);
			MapActiveSirens[iClientID] = L"test";
			PrintUserCmdText(iClientID, L"Police system activated.");
		}
	}

	return true;
}
