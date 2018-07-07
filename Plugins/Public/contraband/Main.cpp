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
map<uint, uint> mapItems;
map<uint, uint> iClient_systems;

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

	iClient_systems.clear(); // Remove client ids

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

		ConPrint(L"Contraband: Houses Loaded: %u \n", housesLoaded);
		ConPrint(L"Contraband: House Systems Loaded: %u \n", houseSystemsLoaded);
		ConPrint(L"Contraband: Laws Loaded: %u \n", lawsLoaded);
		ConPrint(L"Contraband: Illegal Items Loaded: %u \n", contrabandLoaded);
	}

	string goods = R"(..\data\equipment\select_equip.ini)";
	if (ini.open(goods.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("Commodity"))
			{
				uint ids_name;
				uint nick_id;
				while (ini.read_value())
				{
					if (ini.is_value("nickname"))
					{
						nick_id = CreateID(ini.get_value_string());
					}
					else if (ini.is_value("ids_name"))
					{
						ids_name = ini.get_value_int(0);
					}
				}
				mapItems[nick_id] = ids_name;
			}
		}
		ini.close();
		ConPrint(L"Contraband: Loaded Items: %u\n", mapItems.size());
	}
	HkLoadStringDLLs();
	
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

static void CheckCargo(int iClientID, bool manualCargoCheck)
{
	if (set_lawsContrabandEnabled)
	{
		uint iSystemID;
		pub::Player::GetSystem(iClientID, iSystemID); // What system are we in?

		int iHoldSize;
		list<CARGO_INFO> lstCargo;
		HkEnumCargo((const wchar_t*)Players.GetActiveCharacterName(iClientID), lstCargo, iHoldSize); // Get all their cargo

		// If we are currently entering a system that is not within the sector we were in before or if we are just entering a sector from a system that was not in a sector.
		if (
			!manualCargoCheck &&
			(
				( mapHouseAndSystems[iSystemID] != mapHouseAndSystems[iClient_systems[iClientID]] )
				||
				( mapHouseAndSystems.find(iClient_systems[iClientID]) == mapHouseAndSystems.end() )
			)
			||
			(
				manualCargoCheck
				&&
				mapHouseAndSystems.find(iSystemID) != mapHouseAndSystems.end()
				)
			)

		{
			string currentHouse = mapHouseAndSystems[iSystemID]; // What house/sector are we in?
			if(mapHouseCargoList.find(currentHouse) != mapHouseCargoList.end()) // Is the system we are in currently declared as being in any particular sector?
			{
				list<uint> cfgCargoLst = mapHouseCargoList[currentHouse]; // Load in the cargo for the sector we are currently in
				bool foundCargo = false;
				for (list<CARGO_INFO>::iterator i = lstCargo.begin(); i != lstCargo.end(); ++i) // If we are carrying cargo we've declared illegal.
				{
					if(find(cfgCargoLst.begin(), cfgCargoLst.end(), i->iArchID) != cfgCargoLst.end()) // If we found some cargo
					{
						wstring illegalCargo = HkGetWStringFromIDS(mapItems[i->iArchID]); // Get the name of the cargo
						ContrabandWarning(iClientID, L"WARNING: You are carrying %s which is illegal in this sector.", illegalCargo.c_str()); // Inform the criminal scum
						ContrabandWarning(iClientID, L"If you are caught carrying %s, the local authorities might fine or destroy you.", illegalCargo.c_str());
						foundCargo = true;
					}
				}
				if (!foundCargo && manualCargoCheck)
				{
					PrintUserCmdText(iClientID, L"You are not currently carrying any cargo that is considered illegal in this sector.");
				}
			}
		}
		if (manualCargoCheck && mapHouseAndSystems.find(iSystemID) == mapHouseAndSystems.end())
		{
			PrintUserCmdText(iClientID, L"The system you are in is not under the complete control of a single entity.");
		}
	}
}

//////////////////////////////////////////////
//                 Commands                 //
//////////////////////////////////////////////
bool UserCmd_ManualCargoCheck(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage)
{
	bool manualCargoCheck = true;
	CheckCargo(iClientID, manualCargoCheck);
	return true;
}

bool UserCmd_Laws(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage)
{
	if (set_lawsEnabled)
	{
		uint getSystem;
		pub::Player::GetSystem(iClientID, getSystem); // What system are we in?

		if (mapHouseAndSystems.find(getSystem) != mapHouseAndSystems.end()) // If our system is any house/sector
		{
			string currentHouse = mapHouseAndSystems[getSystem]; // Load in the data for that particular house
			if (mapHouseLawList.find(currentHouse) != mapHouseLawList.end()) // If we have the data for the laws of that house
			{
				wstring wscXML = mapHouseLawList[currentHouse]; // Load in the XML
				wstring wscPlayerInfo = L"<RDL><PUSH/>" + wscXML + L"<PARA/><POP/></RDL>"; // Format

				HkChangeIDSString(iClientID, 500001, L"The Local Laws");
				HkChangeIDSString(iClientID, 500000, wscPlayerInfo);

				FmtStr caption(0, 0);
				caption.begin_mad_lib(500001);
				caption.end_mad_lib();

				FmtStr message(0, 0);
				message.begin_mad_lib(500000);
				message.end_mad_lib();

				pub::Player::PopUpDialog(iClientID, caption, message, POPUPDIALOG_BUTTONS_CENTER_OK); // Display
			}
		}
		return false;
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Calls
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall PlayerLaunch_AFTER(unsigned int iShip, unsigned int iClientID)
{
	bool manualCargoCheck = false;
	CheckCargo(iClientID, manualCargoCheck);
	uint iSystemID;
	pub::Player::GetSystem(iClientID, iSystemID);
	iClient_systems[iClientID] = iSystemID; // Update it after everything else is done.
}

void __stdcall JumpInComplete(unsigned int iSystemID, unsigned int ship)
{
	bool manualCargoCheck = false;
	uint iClientID = HkGetClientIDByShip(ship);
	CheckCargo(iClientID, manualCargoCheck);
	iClient_systems[iClientID] = iSystemID; // Update it after everything else is done.
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
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch_AFTER, PLUGIN_HkIServerImpl_PlayerLaunch_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&JumpInComplete, PLUGIN_HkIServerImpl_JumpInComplete_AFTER, 0));

	return p_PI;
}
