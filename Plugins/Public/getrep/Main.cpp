// Rep Plugin - Get ship rep to every faction
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

map<string, uint> factions;
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

	INI_Reader ini;

	string factionpropfile = "..\\data\\initialworld.ini";
	if (ini.open(factionpropfile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("Group"))
			{
				uint ids_name;
				string nickname;
				while (ini.read_value())
				{
					if (ini.is_value("nickname"))
					{
						nickname = ini.get_value_string();
					}
					else if (ini.is_value("ids_name"))
					{
						ids_name = ini.get_value_int(0);
					}

				}
				factions[nickname] = ids_name;
			}
		}
		ini.close();
		ConPrint(L"Rep: Loaded %u factions\n", factions.size());
	}
	HkLoadStringDLLs();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////


bool UserCmd_Rep(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	wstring wscCharName = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
	map<string, float> factionReps;

	for (map<string, uint>::iterator iter = factions.begin(); iter != factions.end(); iter++)
	{
		float fRep = 0.0f;
		string factionName = iter->first;
		uint factionExternal = iter->second;

		wstring Faction = HkGetWStringFromIDS(factionExternal);
		HK_ERROR error;

		if ((error = HkGetRep(wscCharName, stows(factionName), fRep)) != HKE_OK)
		{
			PrintUserCmdText(iClientID, L"ERR %s", HkErrGetText(error).c_str());
			return true;
		}

		factionReps[wstos(Faction)] = fRep;
	}

	// Below code is mostly from stackoverflow
	// https://stackoverflow.com/a/19528891/1306662

	vector<pair<string, float>> mapVector;

	for (auto iterator = factionReps.begin(); iterator != factionReps.end(); ++iterator) {
		mapVector.emplace_back(*iterator);
	}
	sort(mapVector.begin(), mapVector.end(), [=](pair<string, float>& a, pair<string, float>& b)
	{
		return a.second < b.second;
	});

	for each(pair<string, float> Vector in mapVector) {
		string FactionFinal = Vector.first;
		float RepFinal = Vector.second;
		PrintUserCmdText(iClientID, L"Faction: %s — %0.2f", stows(FactionFinal).c_str(), RepFinal);
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Client command processing
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
	{ L"/rep", UserCmd_Rep, L"Usage: /rep" },
	{ L"/rep*", UserCmd_Rep, L"Usage: /rep" },
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
	p_PI->sName = "Getrep by Laz";
	p_PI->sShortName = "getrep";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;
	
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));

	return p_PI;
}
