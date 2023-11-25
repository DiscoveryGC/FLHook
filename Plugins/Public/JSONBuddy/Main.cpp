// AlleyPlugin for FLHookPlugin
// March 2015 by Alley
//
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Includes
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Main.h"
#include <sstream>
#include <iostream>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include "minijson_writer.hpp"

namespace pt = boost::posix_time;

map <uint, ACTIVITY_DATA> mapActivityData;
map <uint, string> mapShips;
map <uint, string> mapIDs;

static int debug = 0;
int jsontimer = 30;

/// A return code to indicate to FLHook if we want the hook processing to continue.
PLUGIN_RETURNCODE returncode;

void LoadSettings();

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	srand((uint)time(0));
	// If we're being loaded from the command line while FLHook is running then
	// set_scCfgFile will not be empty so load the settings as FLHook only
	// calls load settings on FLHook startup and .rehash.
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		if (set_scCfgFile.length() > 0)
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
//Loading Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SpurdoShipzor()
{
	string shiparchfile = "..\\data\\ships\\shiparch.ini";
	string idfile = "..\\data\\equipment\\misc_equip.ini";

	int shipcount = 0;
	int idcount = 0;

	INI_Reader ini;
	if (ini.open(shiparchfile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("Ship"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("nickname"))
					{
						string shipname = ini.get_value_string(0);
						uint shiphash = CreateID(ini.get_value_string(0));

						mapShips[shiphash] = shipname;
						++shipcount;
					}
				}
			}
		}
		ini.close();
		ConPrint(L"JSONBuddy: Loaded %u ships\n", shipcount);
	}
	if (ini.open(idfile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("Tractor"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("nickname"))
					{
						string shipname = ini.get_value_string(0);
						uint shiphash = CreateID(ini.get_value_string(0));

						mapIDs[shiphash] = shipname;
						++idcount;
					}
				}
			}
		}
		ini.close();
		ConPrint(L"JSONBuddy: Loaded %u IDs\n", idcount);
	}
}

void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;
	Condata::LoadSettings();
	SpurdoShipzor();
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define IS_CMD(a) !wscCmd.compare(L##a)

// Admin commands
bool ExecuteCommandString_Callback(CCmds* cmds, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	if (IS_CMD("getstats"))
	{
		struct PlayerData *pPD = 0;
		while (pPD = Players.traverse_active(pPD))
		{
			uint iClientID = HkGetClientIdFromPD(pPD);
			if (HkIsInCharSelectMenu(iClientID))
				continue;


			CDPClientProxy *cdpClient = g_cClientProxyArray[iClientID - 1];
			if (!cdpClient)
				continue;

			int saturation = (int)(cdpClient->GetLinkSaturation() * 100);
			int txqueue = cdpClient->GetSendQSize();
			cmds->Print(L"charname=%s clientid=%u loss=%u lag=%u pingfluct=%u saturation=%u txqueue=%u\n",
				Players.GetActiveCharacterName(iClientID), iClientID,
				ConData[iClientID].iAverageLoss, ConData[iClientID].iLags, ConData[iClientID].iPingFluctuation,
				saturation, txqueue);
		}
		cmds->Print(L"OK\n");
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		return true;
	}
	else if (IS_CMD("kick"))
	{
		// Find by charname. If this fails, fall through to default behaviour.
		CAccount *acc = HkGetAccountByCharname(cmds->ArgCharname(1));
		if (!acc)
			return false;

		// Logout.
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		acc->ForceLogout();
		cmds->Print(L"OK\n");

		// If the client is still active then force the disconnect.
		uint iClientID = HkGetClientIdFromAccount(acc);
		if (iClientID != -1)
		{
			cmds->Print(L"Forcing logout on iClientID=%d\n", iClientID);
			Players.logout(iClientID);
		}
		return true;
	}
	return false;
}

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
	{ L"/ping", Condata::UserCmd_Ping, L"Usage: /ping" },
	{ L"/pingtarget", Condata::UserCmd_PingTarget, L"Usage: /pingtarget" },
	{ L"/pt", Condata::UserCmd_PingTarget, L"Usage: /pingtarget" },
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
//Actual Code
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Clean up when a client disconnects */
void ClearClientInfo(uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	Condata::ClearClientInfo(iClientID);
	mapActivityData.erase(iClientID);
}

void __stdcall PlayerLaunch(unsigned int iShip, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
	Condata::PlayerLaunch(iShip, client);
}

void __stdcall CharacterSelect_AFTER(struct CHARACTER_ID const & cId, unsigned int iClientID)
{
	mapActivityData.erase(iClientID);
	mapActivityData[iClientID].charname = wstos((const wchar_t*)Players.GetActiveCharacterName(iClientID));

	wstring spurdoip;
	HkGetPlayerIP(iClientID, spurdoip);
	mapActivityData[iClientID].ip = wstos(spurdoip);

	Archetype::Ship *ship = Archetype::GetShip(Players[iClientID].iShipArchetype);
	mapActivityData[iClientID].shiparch = "UNKNOWN";
	if (ship)
	{
		mapActivityData[iClientID].shiparch = mapShips[ship->get_id()];
	}
	else
	{
		AddLog("JSONBuddy: CharacterSelect_AFTER: Client %d (%s) logged in with unknown ship archetype %d", iClientID, mapActivityData[iClientID].charname.c_str(), Players[iClientID].iShipArchetype);
	}

	//ensure the ID is empty, as we want to return something if no ID is found.
	mapActivityData[iClientID].id = "";

	for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
	{
		if (mapIDs.find(item->iArchID) != mapIDs.end())
		{
			if (item->bMounted)
			{
				mapActivityData[iClientID].id = mapIDs[item->iArchID];
				//PrintUserCmdText(iClientID, L"DEBUG: Found relevant item: %s", stows(mapIDs[item->equip.iArchID]).c_str());
				//pub::Audio::PlaySoundEffect(iClientID, CreateID("cargo_jettison"));			
			}
		}
	}
}

void _stdcall Disconnect(unsigned int iClientID, enum EFLConnection p2)
{
	returncode = DEFAULT_RETURNCODE;
	ClearClientInfo(iClientID);
}

void HkTimerJSON()
{
	returncode = DEFAULT_RETURNCODE;
	Condata::HkTimerCheckKick();

	//update activity once per minute
	if (!jsontimer)
	{
		//ConPrint(L"JSONBuddy: Attempting to send data\n");
		stringstream stream;
		minijson::object_writer writer(stream);
		writer.write("timestamp", pt::to_iso_string(pt::second_clock::local_time()));

		string sPlayer = "players";

		minijson::object_writer pwc = writer.nested_object(sPlayer.c_str());

		struct PlayerData *pPD = 0;

		while (pPD = Players.traverse_active(pPD))
		{
			uint iClientID = HkGetClientIdFromPD(pPD);
			if (HkIsInCharSelectMenu(iClientID))
				continue;

			CDPClientProxy *cdpClient = g_cClientProxyArray[iClientID - 1];
			if (!cdpClient)
				continue;

			uint iSystemID;
			pub::Player::GetSystem(iClientID, iSystemID);
			const Universe::ISystem *iSys = Universe::get_system(iSystemID);

			string sysname = iSys->nickname;

			//if it's empty, it's probably a plugin reload. We fill the data so we don't send dumb shit or worse cause exceptions
			if (mapActivityData[iClientID].charname.empty())
			{
				mapActivityData[iClientID].charname = wstos((const wchar_t*)Players.GetActiveCharacterName(iClientID));

				wstring spurdoip;
				HkGetPlayerIP(iClientID, spurdoip);
				mapActivityData[iClientID].ip = wstos(spurdoip);
			}

			Archetype::Ship *ship = Archetype::GetShip(Players[iClientID].iShipArchetype);
			mapActivityData[iClientID].shiparch = "UNKNOWN";
			if (ship) {
				mapActivityData[iClientID].shiparch = mapShips[ship->get_id()];
			}

			//ensure the ID is empty, as we want to return something if no ID is found.
			mapActivityData[iClientID].id = "";

			for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
			{
				if (mapIDs.find(item->iArchID) != mapIDs.end())
				{
					if (item->bMounted)
					{
						mapActivityData[iClientID].id = mapIDs[item->iArchID];
						//PrintUserCmdText(iClientID, L"DEBUG: Found relevant item: %s", stows(mapIDs[item->equip.iArchID]).c_str());
						//pub::Audio::PlaySoundEffect(iClientID, CreateID("cargo_jettison"));
					}
				}
			}

			minijson::object_writer pw = pwc.nested_object(mapActivityData[iClientID].charname.c_str());
			pw.write("system", sysname);
			pw.write("ip", mapActivityData[iClientID].ip.c_str());
			pw.write("ship", mapActivityData[iClientID].shiparch.c_str());
			pw.write("id", mapActivityData[iClientID].id.c_str());
			pw.write("ping", ConData[iClientID].iAveragePing);
			pw.write("loss", ConData[iClientID].iAverageLoss);
			pw.write("lag", ConData[iClientID].iLags);
			pw.close();


		}
		pwc.close();
		writer.close();

		//dump to a file
		FILE *file = fopen("c:/stats/player_status.json", "w");
		if (file)
		{
			fprintf(file, "%s", stream.str().c_str());
			fclose(file);
		}

		jsontimer = 30;
	}
	else
	{
		jsontimer = jsontimer - 1;
	}
}

void UserCmd_Help(uint iClientID, const wstring &wscParam)
{
	returncode = DEFAULT_RETURNCODE;
	Condata::UserCmd_Help(iClientID, wscParam);
}

void SPObjUpdate(struct SSPObjUpdateInfo const &ui, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	Condata::SPObjUpdate(ui, iClientID);
}

int Update()
{
	returncode = DEFAULT_RETURNCODE;
	Condata::Update();
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions to hook
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "JSONBuddy by Alley";
	p_PI->sShortName = "jsonbuddy";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkTimerJSON, PLUGIN_HkTimerCheckKick, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ExecuteCommandString_Callback, PLUGIN_ExecuteCommandString_Callback, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch, PLUGIN_HkIServerImpl_PlayerLaunch_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ClearClientInfo, PLUGIN_ClearClientInfo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterSelect_AFTER, PLUGIN_HkIServerImpl_CharacterSelect_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Disconnect, PLUGIN_HkIServerImpl_DisConnect, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Help, PLUGIN_UserCmd_Help, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Update, PLUGIN_HkIServerImpl_Update, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SPObjUpdate, PLUGIN_HkIServerImpl_SPObjUpdate, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));

	return p_PI;
}