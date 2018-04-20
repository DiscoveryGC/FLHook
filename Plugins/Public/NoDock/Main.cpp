// NoDock Expanded (This plugin is an expansion on the original written by Alley)
// April 2018 by Laz, orginal concept and code written by Alley
//
// This plugin offers the ability for certain IDs to restrict other IDs from docking on their bases
// and stop other IDs all together from using certain solar objects, like bases, jumpgates, etc...
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
#include <sstream>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Are we in debug mode
static int iDebug = 0;

// Default Duration (in seconds) for nodock
static int duration = 120;

// Is the plugin enabled
static bool bPluginEnabled;

// A list of all our INI sections
list<INISECTIONVALUE> INIList;

// A list of all our IDs
list<string> lstIDs;

// A map of all the ships with permission to use the command or are restricted from docking | <iClientID, iShipID>
map<uint, uint> mapShips;

// A map of our ID internal name and all that it can no dock | <ID, <SpaceObj>>
map<uint, list<uint>> mapIDRestrictions; 

// A map of our internal name and all that it is restricted from docking with | <ID, <SpaceObj>>
map<uint, list<uint>> mapIDRestricted;

// A map of active nodocks | <iClientID, <ID, Duration>>
map<uint, map<uint, int>> mapActiveNoDocks;

// List of ships ignored by the /nodock command (stop people nodocking admins)
list<wstring> lstIgnoredShips;

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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Logging
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
//Loading Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;
	int noDockIDS = 0; // How many IDs are we going to have
	int noDockedBases = 0; // How many bases can be nodocked
	int restrictedCount = 0; // How many solars are restricted
	int ignoredShips = 0; // How many ships are ignored by nodock

	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string NoDockCfg = string(szCurDir) + R"(\flhook_plugins\laz_NoDock.cfg)"; // Our Config File

	bPluginEnabled = IniGetB(NoDockCfg, "General", "enabled", false); // Is the plugin enabled in the config? (No by default)
	iDebug = IniGetI(NoDockCfg, "General", "debug", 0); // Are we in debug mode?
	duration = IniGetI(NoDockCfg, "General", "duration", duration); // How long do we want the /nodock command to last
	IniGetSection(NoDockCfg, "ID List", INIList);

	for (auto& iter : INIList) // Iterate through all the enteries in the "ID List" section
	{
		lstIDs.emplace_back(iter.scKey); // Create a list of all the ID sections we want
		noDockIDS++;
	}

	INI_Reader ini;
	if (ini.open(NoDockCfg.c_str(), false))
	{
		while (ini.read_header())
		{
			for (auto& iter : lstIDs) // Iterate through all the sections we created
			{
				if (ini.is_header(iter.c_str())) // If the section header is in list of section headers we created
				{
					list<uint> nodockSolars; // An empty list to store the nodockable solars
					list<uint> restrictedSolars; // An empty list to store the solars an ID will be restricted from
					uint ID; // Our ID, aka license
					while (ini.read_value())
					{
						if(ini.is_value("ID"))
						{
							ID = CreateID(ini.get_value_string()); 
						}
						else if(ini.is_value("nodock"))
						{
							noDockedBases++;
							nodockSolars.emplace_back(CreateID(ini.get_value_string()));
						}
						else if(ini.is_value("restricted"))
						{
							restrictedCount++;
							restrictedSolars.emplace_back(CreateID(ini.get_value_string()));
						}
					}
					mapIDRestrictions[ID] = nodockSolars;
					mapIDRestricted[ID] = restrictedSolars;
				}
				else if(ini.is_header("Ignored Ships"))
				{
					while(ini.read_value())
					{
						if(ini.is_value("name"))
						{
							ignoredShips++;
							lstIgnoredShips.emplace_back((const wchar_t*)ini.get_value_wstring());
						}
					}
				}
			}
		}
		ini.close();
	}

	ConPrint(L"No Dock: Loaded %u IDs\n", noDockIDS);
	ConPrint(L"No Dock: Loaded %u Nodockable Solars\n", noDockedBases);
	ConPrint(L"No Dock: Loaded %u Restricted Solars\n", restrictedCount);
	ConPrint(L"No Dock: Loaded %u Ignored Ships\n", ignoredShips);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Commands
///////////////////////////////////////////////////////////////////////////////////////////////////////////////


bool UserCmd_NoDock(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginEnabled)
	{
		if (iDebug > 0)
		{
			PrintUserCmdText(iClientID, L"The Nodock plugin is currently disabled.");
		}
		return true;
	}

	wstring wscCharacterName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID));
	bool correctID = false;
	uint noDocker; // The ID that preformed the nodock

	for (auto& mapShip : mapShips)
	{
		if(mapShip.first == iClientID)
		{
			correctID = true;
			noDocker = mapShip.second;
			break;
		}
	}

	if(correctID)
	{
		if(iDebug > 0)
		{
			PrintUserCmdText(iClientID, L"Debug: You are allowed to use this.");
		}

		uint iShip = 0;
		pub::Player::GetShip(iClientID, iShip);
		if (!iShip) {
			PrintUserCmdText(iClientID, L"ERR: You cannot use this when docked.");
			return true;
		}

		uint iTarget = 0;
		pub::SpaceObj::GetTarget(iShip, iTarget);

		if (!iTarget) {
			PrintUserCmdText(iClientID, L"ERR: You do not have a target selected.");
			return true;
		}

		const uint iClientIDTarget = HkGetClientIDByShip(iTarget);
		if (!HkIsValidClientID(iClientIDTarget))
		{
			PrintUserCmdText(iClientID, L"ERR: Target is not a player.");
			return true;
		}

		wstring wscTargetCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientIDTarget);

		for (auto& i : mapActiveNoDocks)
		{
			if (i.first == iClientIDTarget)
			{
				PrintUserCmdText(iClientID, L"OK Removal of docking rights reset to %d seconds", duration);
				PrintUserCmdText(iClientIDTarget, L"Removal of docking rights reset to %d seconds", duration);
				for(auto& ii : i.second)
				{
					ii.second = duration;
				}
				return true;
			}
		}

		mapActiveNoDocks[iClientIDTarget][noDocker] = duration;

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
		wscMsgLog = ReplaceStr(wscMsgLog, L"%sender", wscCharacterName.c_str());
		wscMsgLog = ReplaceStr(wscMsgLog, L"%victim", wscTargetCharname.c_str());
		string scText = wstos(wscMsgLog);
		Logging("%s", scText.c_str());

		return true;
	}

	PrintUserCmdText(iClientID, L"You do not have the correct ID to use this command!");
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual Code
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Clean up when a client disconnects */
void ClearClientInfo(uint iClientID)
{
	mapActiveNoDocks.erase(iClientID);
	mapShips.erase(iClientID);
	returncode = DEFAULT_RETURNCODE;
}

bool NoDocked(uint iShipID, uint iTarget, uint iClientID)
{
	wstring wscCharacterName = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
	if (find(begin(lstIgnoredShips), end(lstIgnoredShips), wscCharacterName) != end(lstIgnoredShips))
	{
		PrintUserCmdText(iClientID, L"You are immune to the nodock effect.");
		return false;
	}

	bool noDocked = false; // By default no ship is "Nodocked"
	uint iDockwithID; // Empty uint
	pub::SpaceObj::GetDockingTarget(iTarget, iDockwithID); // 

	if (mapShips.find(iClientID) != mapShips.end())
	{
		for (auto& mapShip : mapShips)
		{
			for (auto& ii : mapIDRestricted)
			{
				if (mapShip.second == ii.first)
				{
					for(auto iter = ii.second.begin(); (iter != ii.second.end()); ++iter)
					{
						if (iTarget == *iter || iDockwithID == *iter)
						{
							pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("info_access_denied"));
							PrintUserCmdText(iClientID, L"You do not have the correct docking codes to access this structure.");
							return true;
						}
					}
				}
			}
		}
	}

	//
	if(mapActiveNoDocks.find(iClientID) != mapActiveNoDocks.end())
	{
		if(iDebug > 0)
		{
			PrintUserCmdText(iClientID, L"You are currently under the nodock effect - some bases are off limits.");
		}
		noDocked = true;
	}

	if(!noDocked)
	{
		if (iDebug > 1)
		{
			PrintUserCmdText(iClientID, L"Debug: You are not nodocked.");
		}
		return false;
	}

	if(noDocked)
	{
		for (auto& mapActiveNoDock : mapActiveNoDocks)
		{
			for (auto i = mapActiveNoDock.second.begin(); i != mapActiveNoDock.second.end(); i++)
			{
				uint noDocker = i->first; // The id of the guy who nodocked the ship
				int timeLeft = i->second; // How long will it last?

				for (auto ii = mapIDRestrictions.begin(); ii != mapIDRestrictions.end(); ii++)
				{
					if(ii->first == noDocker) // Get only the list that applies to the ID of the guy who initated the nodock.
					{
						foreach(ii->second, uint, iter) // Iterate over the list of bases to see if they are docking with one in the list
						{
							if (iTarget == *iter || iDockwithID == *iter) // If the base nickname or "dockwith" element is listed in the config, deny.
							{
								pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("info_access_denied")); // Play the sound "Dock Denied" to the user that tried to dock.
								wstring wscMsg = L"%victim is attempting to dock while their rights are currently suspended. %victim must wait %time seconds before rights are restored.";
								wscMsg = ReplaceStr(wscMsg, L"%time", to_wstring(timeLeft)); // Convert "%time" to the amount of time left
								wscMsg = ReplaceStr(wscMsg, L"%victim", (const wchar_t*)Players.GetActiveCharacterName(iClientID)); // Replace with character name
								PrintLocalUserCmdText(i->first, wscMsg, 10000); PrintUserCmdText(iClientID, L"", timeLeft); // Inform the locals that the criminal scum is trying to dock
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}

void Timer()
{
	for (auto i = mapActiveNoDocks.begin(); i != mapActiveNoDocks.end(); i++)
	{
		for (auto ii = i->second.begin(); ii != i->second.end(); ii++)
		{
			if (ii->second < 1)
			{
				wstring wscMsg = L"%time %victim's docking rights have been restored.";
				wscMsg = ReplaceStr(wscMsg, L"%time", GetTimeString(false));
				wscMsg = ReplaceStr(wscMsg, L"%victim", (const wchar_t*)Players.GetActiveCharacterName(i->first));
				PrintLocalUserCmdText(i->first, wscMsg, 10000);
				mapActiveNoDocks.erase(i->first);
			}
			else
			{
				ii->second = ii->second - 1;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Calls
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall PlayerLaunch(unsigned int iShip, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	int iHoldSize;
	list<CARGO_INFO> lstCargo;
	HkEnumCargo((const wchar_t*)Players.GetActiveCharacterName(iClientID), lstCargo, iHoldSize);

	for (auto ii : lstCargo)
	{
		auto* i = &ii;
		if(i->bMounted)
		{
			for (auto& iter : mapIDRestricted)
			{
				if(iter.first == i->iArchID)
				{
					mapShips[iClientID] = i->iArchID;
					return;
				}
			}
		}
	}
}

void __stdcall Disconnect(unsigned int iClientID, enum  EFLConnection state)
{
	returncode = DEFAULT_RETURNCODE;
	ClearClientInfo(iClientID);
}

void __stdcall CharacterSelect_AFTER(struct CHARACTER_ID const &charId, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	ClearClientInfo(iClientID);
}

int __cdecl Dock_Call(unsigned int const &iShipID, unsigned int const &iTarget, int iCancel, enum DOCK_HOST_RESPONSE response)
{
	returncode = DEFAULT_RETURNCODE;

	uint iClientID = HkGetClientIDByShip(iShipID);

	if (iClientID && (response == PROCEED_DOCK || response == DOCK) && iCancel != -1)
	{
		bool isNoDocked = NoDocked(iShipID, iTarget, iClientID);
		if (isNoDocked)
		{
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
			return 0;
		}
		if(!isNoDocked)
		{
			returncode = DEFAULT_RETURNCODE;
			return 1;
		}
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Client command processing
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef bool(*_UserCmdProc)(uint, const wstring &, const wstring &, const wchar_t*);

struct USERCMD
{
	wchar_t *wszCmd;
	_UserCmdProc proc;
	wchar_t *usage;
};

USERCMD UserCmds[] =
{
	{ L"/nodock", UserCmd_NoDock, L"Usage: /nodock" },
	{ L"/nodock*", UserCmd_NoDock, L"Usage: /nodock" },
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
// Functions to hook
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "NoDock by Laz using code from Alley";
	p_PI->sShortName = "nodock";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;
	
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ClearClientInfo, PLUGIN_ClearClientInfo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch, PLUGIN_HkIServerImpl_PlayerLaunch, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Disconnect, PLUGIN_HkIServerImpl_DisConnect, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterSelect_AFTER, PLUGIN_HkIServerImpl_CharacterSelect_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Dock_Call, PLUGIN_HkCb_Dock_Call, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Timer, PLUGIN_HkTimerCheckKick, 0));

	return p_PI;
}
