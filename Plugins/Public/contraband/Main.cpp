// Contraband - Alert ships when they carry restricted goods in certain systems
// By Laz, w/ help from @Alex.
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
#include <list>
#include <map>
#include <algorithm>
#include <FLHook.h>
#include <plugin.h>
#include <PluginUtilities.h>
#include "Main.h"

#include "../hookext_plugin/hookext_exports.h"

#define POPUPDIALOG_BUTTONS_LEFT_YES 1
#define POPUPDIALOG_BUTTONS_CENTER_NO 2
#define POPUPDIALOG_BUTTONS_RIGHT_LATER 4
#define POPUPDIALOG_BUTTONS_CENTER_OK 8

static bool set_lawsEnabled;
static bool set_lawsContrabandEnabled;

list<INISECTIONVALUE> lstHouseTag;
list<string> lstHouse;

map<uint, string> mapHouseAndSystems;
map<string, wstring> mapHouseLawList;
map<string, list<uint>> mapHouseCargoList;

/// A return code to indicate to FLHook if we want the hook processing to continue.
PLUGIN_RETURNCODE returncode;

void LoadSettings();

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	srand((uint)time(0));
	// If we're being loaded from the command line while FLHook is running then
	// set_scCfgFile will not be empty so load the settings as FLHook only
	// calls load settings on FLHook startup and .rehash.
	if(fdwReason == DLL_PROCESS_ATTACH)
	{
		if (set_scCfgFile.length()>0)
			LoadSettings();
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
	}
	return true;
}

/// Hook will call this function after calling a plugin function to see if we the
/// processing to continue
EXPORT PLUGIN_RETURNCODE Get_PluginReturnCode()
{
	return returncode;
}

bool bPluginEnabled = true;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Loading Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	int housesLoaded = 0;
	int houseSystemsLoaded = 0;
	int lawsLoaded = 0;
	int contrabandLoaded = 0;

	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\laz_laws.cfg";

	set_lawsEnabled = IniGetB(scPluginCfgFile, "LawsConfig", "Enabled", true);
	set_lawsContrabandEnabled = IniGetB(scPluginCfgFile, "ContrabandConfig", "Enabled", true);
	IniGetSection(scPluginCfgFile, "Houses", lstHouseTag);

	foreach(lstHouseTag, INISECTIONVALUE, iter)
	{
		lstHouse.push_back(iter->scKey);
		housesLoaded++;
	}

	INI_Reader ini;
	if (ini.open(scPluginCfgFile.c_str(), false))
	{
		while (ini.read_header())
		{
			foreach(lstHouse, string, iter)
			{
				string scIter = *iter;
				if (ini.is_header(scIter.c_str()))
				{
					while (ini.read_value())
					{
						if (ini.is_value("system"))
						{
							mapHouseAndSystems[CreateID(ini.get_value_string(0))] = scIter;
							houseSystemsLoaded++;
						}
						else if (ini.is_value("contraband"))
						{
							mapHouseCargoList[scIter].push_back(CreateID(ini.get_value_string(0)));
							contrabandLoaded++;
						}
					}
				}
				else if (ini.is_header("Laws"))
				{
					while (ini.read_value())
					{
						if (ini.is_value("house"))
						{
							mapHouseLawList[ini.get_value_string(0)] = stows(ini.get_value_string(1));
							lawsLoaded++;
						}
					}
				}
			}
		}
		ini.close();

		ConPrint(L"LAWS: Houses Loaded: %u \n", housesLoaded);
		ConPrint(L"LAWS: House Systems Loaded: %u \n", houseSystemsLoaded);
		ConPrint(L"LAWS: Laws Loaded: %u \n", lawsLoaded);
		ConPrint(L"LAWS: Contraband Items Loaded: %u \n", contrabandLoaded);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ContrabandWarning(uint iClientID, wstring wscText, ...) // WE WANT BOLD RED
{
	wchar_t wszBuf[1024 * 8] = L"";
	va_list marker;
	va_start(marker, wscText);
	_vsnwprintf(wszBuf, sizeof(wszBuf) - 1, wscText.c_str(), marker);

	wstring wscXML = L"<TRA data=\"65281\" mask=\"-1\"/><TEXT>" + XMLText(wszBuf) + L"</TEXT>";
	HkFMsg(iClientID, wscXML);
}

static void CheckCargo(int iClientID)
{
	if (set_lawsContrabandEnabled)
	{
		uint iSystemID;
		pub::Player::GetSystem(iClientID, iSystemID);

		int iHoldSize;
		list<CARGO_INFO> lstCargo;
		HkEnumCargo((const wchar_t*)Players.GetActiveCharacterName(iClientID), lstCargo, iHoldSize);

		for (map<uint, string>::iterator iter = mapHouseAndSystems.begin(); iter != mapHouseAndSystems.end(); iter++)
		{

			uint currentSector = iter->first;
			string currentHouse = iter->second;

			if (currentSector == iSystemID)
			{
				for (map<string, list<uint>>::iterator iter = mapHouseCargoList.begin(); iter != mapHouseCargoList.end(); iter++)
				{
					string currentHouseCompare = iter->first;
					list<uint> cfgCargoLst = iter->second;

					if (currentHouse == currentHouseCompare)
					{
						for (list<CARGO_INFO>::iterator i = lstCargo.begin(); i != lstCargo.end(); ++i)
						{
							uint correctCargo = i->iArchID;
							foreach(cfgCargoLst, uint, cfgCargoIter)
							{
								if (*cfgCargoIter == correctCargo) // If their cargo is on the list
								{
									ContrabandWarning(iClientID, L"WARNING: You are carrying items which are illegal in this sector.");
									return;
								}
							}
						}
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////
//                 Commands                 //
//////////////////////////////////////////////
bool UserCmd_ManualCargoCheck(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage)
{
	CheckCargo(iClientID);
	return true;
}

bool UserCmd_Laws(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage)
{
	if (set_lawsEnabled)
	{
		for (map<uint, string>::iterator iter = mapHouseAndSystems.begin(); iter != mapHouseAndSystems.end(); iter++)
		{
			uint currentSystem = iter->first;
			string currentHouse = iter->second;

			uint getSystem;
			pub::Player::GetSystem(iClientID, getSystem);

			if (currentSystem == getSystem)
			{
				for (map<string, wstring>::iterator iter2 = mapHouseLawList.begin(); iter2 != mapHouseLawList.end(); iter2++)
				{
					string currentHouseCompare = iter2->first;
					if (currentHouse == currentHouseCompare)
					{
						wstring scXML = iter2->second;

						wstring wscPlayerInfo = L"<RDL><PUSH/>" + scXML + L"<PARA/><POP/></RDL>";

						struct PlayerData *pPD = 0;
						while (pPD = Players.traverse_active(pPD))
						{
							uint iClientID = HkGetClientIdFromPD(pPD);

							HkChangeIDSString(iClientID, 66076, wscPlayerInfo);
							HkChangeIDSString(iClientID, 1, L"The Local Laws");

							FmtStr caption(0, 0);
							caption.begin_mad_lib(1);
							caption.end_mad_lib();

							FmtStr message(0, 0);
							message.begin_mad_lib(66076);
							message.end_mad_lib();

							pub::Player::PopUpDialog(iClientID, caption, message, POPUPDIALOG_BUTTONS_CENTER_OK);
						}
					}
				}

			}
		}
		return false;
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Calls
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall PlayerLaunch_AFTER(unsigned int iShip, unsigned int client)
{
	CheckCargo(client);
}

void __stdcall JumpInComplete(unsigned int system, unsigned int ship)
{
	uint iClientID = HkGetClientIDByShip(ship);
	CheckCargo(iClientID);
}

/** Clean up when a client disconnects */
void ClearClientInfo(uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Client command processing
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef bool(*_UserCmdProc)(uint, const wstring &, const wstring &, const wchar_t*);

// ReSharper disable CppInconsistentNaming
struct USERCMD
	// ReSharper restore CppInconsistentNaming
{
	wchar_t *wszCmd;
	_UserCmdProc proc;
	wchar_t *usage;
};

USERCMD UserCmds[] =
{
	{ L"/laws", UserCmd_Laws, L"Usage: /laws" },
	{ L"/laws*", UserCmd_Laws, L"Usage: /laws" },
	{ L"/checkcargo", UserCmd_ManualCargoCheck, L"Usage: /checkcargo" },
	{ L"/checkcargo*", UserCmd_ManualCargoCheck, L"Usage: /checkcargo" },
};

/**
This function is called by FLHook when a user types a chat string. We look at the
string they've typed and see if it starts with one of the above commands. If it
does we try to process it.
*/
bool UserCmd_Process(uint iClientID, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	wstring wscCmdLineLower = ToLower(wscCmd);

	// If the chat string does not match the USER_CMD then we do not handle the
	// command, so let other plugins or FLHook kick in. We require an exact match
	for (uint i = 0; (i < sizeof(UserCmds) / sizeof(USERCMD)); i++)
	{

		if (wscCmdLineLower.find(UserCmds[i].wszCmd) == 0)
		{
			// Extract the parameters string from the chat string. It should
			// be immediately after the command and a space.
			wstring wscParam = L"";
			if (wscCmd.length() > wcslen(UserCmds[i].wszCmd))
			{
				if (wscCmd[wcslen(UserCmds[i].wszCmd)] != ' ')
					continue;
				wscParam = wscCmd.substr(wcslen(UserCmds[i].wszCmd) + 1);
			}

			// Dispatch the command to the appropriate processing function.
			if (UserCmds[i].proc(iClientID, wscCmd, wscParam, UserCmds[i].usage))
			{
				// We handled the command tell FL hook to stop processing this
				// chat string.
				returncode = SKIPPLUGINS_NOFUNCTIONCALL; // we handled the command, return immediatly
				return true;
			}
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions to hook
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "Contraband.dll by Laz";
	p_PI->sShortName = "contraband";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;
	
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ClearClientInfo, PLUGIN_ClearClientInfo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch_AFTER, PLUGIN_HkIServerImpl_PlayerLaunch_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&JumpInComplete, PLUGIN_HkIServerImpl_JumpInComplete_AFTER, 0));

	return p_PI;
}
