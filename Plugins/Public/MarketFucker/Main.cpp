// AlleyPlugin for FLHookPlugin
// March 2015 by Alley
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
#include <sstream>
#include <iostream>


static int set_iPluginDebug = 0;

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

// For ships, we go the easy way and map each ship belonging to each base
static map <uint, list<uint>> mapACShips;
// For items, we create a list of all bases with market possibilities.
// And a list of the items we keep under watch
static list<uint> mapACBases;
static map <uint, string> mapACItems;

struct sstr
{
	map <uint, int> items;
};
// map we'll use to keep track of watched item sales.
static map <uint, sstr> mapACSales;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Loading Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	// The path to the configuration file.
	string marketshipsfile = "..\\data\\equipment\\market_ships.ini";
	string marketcommoditiesfile = "..\\data\\equipment\\market_commodities.ini";
	string flhookitems = "..\\exe\\flhook_plugins\\alley_mf.cfg";
	int shipamount1 = 0;
	int shipamount2 = 0;
	int commodamount1 = 0;
	int commodamount2 = 0;

	INI_Reader ini;
	if (ini.open(marketshipsfile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("BaseGood"))
			{
				while (ini.read_value())
				{
					uint currentbase;
					if (ini.is_value("base"))
					{
						currentbase = CreateID(ini.get_value_string(0));
						shipamount1 = shipamount1 + 1;
					}
					else if (ini.is_value("marketgood"))
					{
						mapACShips[currentbase].push_back(CreateID(ini.get_value_string(0)));
						shipamount2 = shipamount2 + 1;
					}
				}
			}
		}
		ini.close();
	}
	if (ini.open(marketcommoditiesfile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("BaseGood"))
			{
				while (ini.read_value())
				{
					//uint temple = CreateID("st04_03_base");
					uint bastilleguard = CreateID("iw09_03_base");
					uint currentbase;
					if (ini.is_value("base"))
					{
						currentbase = CreateID(ini.get_value_string(0));
						//we don't record operations from the temple of the damned so when we come across it we'll ignore it.
						//if (currentbase == temple)
						//{							
						//	ConPrint(L"MARKETFUCKER: Ignoring Temple of the Damned. \n");		
						//}
						if (currentbase == bastilleguard)
						{
							ConPrint(L"MARKETFUCKER: Ignoring Bastille Guard Station. \n");
						}
						else
						{
							mapACBases.push_back(currentbase);
							commodamount1 = commodamount1 + 1;
						}
					}
				}
			}
		}
		ini.close();
	}
	if (ini.open(flhookitems.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("items"))
			{
				while (ini.read_value())
				{
					uint currentitem;
					if (ini.is_value("item"))
					{
						currentitem = CreateID(ini.get_value_string(0));
						//mapACItems.push_back(currentitem);
						mapACItems[currentitem] = ini.get_value_string(0);
						commodamount2 = commodamount2 + 1;
					}
				}
			}
		}
		ini.close();
	}

	ConPrint(L"MARKETFUCKER: Loaded %u ships for %u bases \n", shipamount2, shipamount1);
	ConPrint(L"MARKETFUCKER: Loaded %u items for %u bases \n", commodamount2, commodamount1);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define IS_CMD(a) !wscCmd.compare(L##a)

// Admin commands
bool ExecuteCommandString_Callback(CCmds* cmds, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	if (IS_CMD("showmarket"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		//AdminCmd_GenerateID(cmds, cmds->ArgStrToEnd(1));
		return true;
	}

	return false;
}

FILE *Logfile = fopen("./flhook_logs/marketfucker.log", "at");

void Logging(const char *szString, ...)
{
	char szBufString[1024];
	va_list marker;
	va_start(marker, szString);
	_vsnprintf(szBufString, sizeof(szBufString) - 1, szString, marker);

	char szBuf[64];
	time_t tNow = time(0);
	struct tm *t = localtime(&tNow);
	strftime(szBuf, sizeof(szBuf), "%d/%m/%Y %H:%M:%S", t);
	fprintf(Logfile, "%s %s\n", szBuf, szBufString);
	fflush(Logfile);
	fclose(Logfile);
	Logfile = fopen("./flhook_logs/marketfucker.log", "at");
}

void LogCheater(uint client, const wstring &reason)
{
	CAccount *acc = Players.FindAccountFromClientID(client);

	if (!HkIsValidClientID(client) || !acc)
	{
		AddLog("ERROR: invalid parameter in log cheater, clientid=%u acc=%08x reason=%s", client, acc, wstos(reason).c_str());
		return;
	}

	//internal log
	string scText = wstos(reason);
	Logging("%s", scText.c_str());

	// Set the kick timer to kick this player. We do this to break potential
	// stack corruption.
	HkDelayedKick(client, 1);

	// Ban the account.
	flstr *flStr = CreateWString(acc->wszAccID);
	Players.BanAccount(*flStr, true);
	FreeWString(flStr);

	// Overwrite the ban file so that it contains the ban reason
	wstring wscDir;
	HkGetAccountDirName(acc, wscDir);
	string scBanPath = scAcctPath + wstos(wscDir) + "\\banned";
	FILE *file = fopen(scBanPath.c_str(), "wb");
	if (file)
	{
		fprintf(file, "Autobanned by Marketfucker\n");
		fclose(file);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Actual Code
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Clean up when a client disconnects */
void ClearClientInfo(uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	mapACSales.erase(iClientID);
}


void __stdcall GFGoodSell(struct SGFGoodSellInfo const &gsi, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	const GoodInfo *packageInfo = GoodList::find_by_id(gsi.iArchID);
	// check for equipments only
	if (packageInfo->iType == 1)
	{
		uint iBase;
		pub::Player::GetBase(iClientID, iBase);

		//in this case, it's more efficent to check if it's an item under watch first.
		for (map<uint, string>::iterator iter = mapACItems.begin(); iter != mapACItems.end(); iter++)
		{
			if (iter->first == gsi.iArchID)
			{
				//PrintUserCmdText(iClientID, L"I have found commodity %s.", stows(iter->second).c_str());
				//We iterate through the base names to see if it's a non-POB base
				list<uint>::iterator i = mapACBases.begin();
				while (i != mapACBases.end())
				{
					if (*i == iBase)
					{
						wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
						wstring wscBaseName = HkGetBaseNickByID(iBase);


						//PrintUserCmdText(iClientID, L"I have found this base, logging the purchase.");
						// check if this item is already under watch, if so increase amount by 1
						if (mapACSales[iClientID].items.find(gsi.iArchID) != mapACSales[iClientID].items.end())
						{
							++mapACSales[iClientID].items.find(gsi.iArchID)->second;
							//PrintUserCmdText(iClientID, L"DEBUG: I have logged %i sales.", mapACSales[iClientID].items.find(gsi.iArchID)->second);
							wstring wscMsgLog = L"<%sender> has sold <%item> to base <%basename> (Already recorded %isale sales of this item)";
							wscMsgLog = ReplaceStr(wscMsgLog, L"%sender", wscCharname.c_str());
							wscMsgLog = ReplaceStr(wscMsgLog, L"%basename", wscBaseName.c_str());
							wscMsgLog = ReplaceStr(wscMsgLog, L"%item", stows(iter->second).c_str());
							wscMsgLog = ReplaceStr(wscMsgLog, L"%isale", stows(itos(mapACSales[iClientID].items.find(gsi.iArchID)->second))).c_str();
							string scText = wstos(wscMsgLog);
							Logging("%s", scText.c_str());

						}
						else
						{
							mapACSales[iClientID].items[gsi.iArchID] = 1;
							wstring wscMsgLog = L"<%sender> has sold <%item> to base <%basename> (First sale)";
							wscMsgLog = ReplaceStr(wscMsgLog, L"%sender", wscCharname.c_str());
							wscMsgLog = ReplaceStr(wscMsgLog, L"%basename", wscBaseName.c_str());
							wscMsgLog = ReplaceStr(wscMsgLog, L"%item", stows(iter->second).c_str());
							string scText = wstos(wscMsgLog);
							Logging("%s", scText.c_str());
						}
						break;
					}
					i++;
				}
				break;
			}
		}
	}
}

void __stdcall GFGoodBuy(struct SGFGoodBuyInfo const &gbi, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	const GoodInfo *packageInfo = GoodList::find_by_id(gbi.iGoodID);
	/*
	if (packageInfo->iType == 0)
	{
	PrintUserCmdText(iClientID, L"This should be a commodity (equipment?) purchase");
	}
	*/
	if (packageInfo->iType == 1)
	{
		//PrintUserCmdText(iClientID, L"This should be an equipment (maybe?) purchase");
		uint iBase;
		pub::Player::GetBase(iClientID, iBase);
		bool aminiceitem = true;
		string itemname;
		int wearecool = 0;

		//in this case, it's more efficent to check if it's an item under watch first.

		for (map<uint, string>::iterator iter = mapACItems.begin(); iter != mapACItems.end(); iter++)
		{
			if (iter->first == gbi.iGoodID)
			{
				//PrintUserCmdText(iClientID, L"I have found this commodity");
				//We iterate through the base names to see if it's a non-POB base
				list<uint>::iterator i = mapACBases.begin();
				while (i != mapACBases.end())
				{
					if (*i == iBase)
					{

						if (mapACSales[iClientID].items.find(gbi.iGoodID) != mapACSales[iClientID].items.end())
						{
							--mapACSales[iClientID].items.find(gbi.iGoodID)->second;

							//PrintUserCmdText(iClientID, L"DEBUG: I have found this sale, letting the purchase go through.");
							aminiceitem = true;
							wearecool = 1;

							wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
							wstring wscBaseName = HkGetBaseNickByID(iBase);

							//PrintUserCmdText(iClientID, L"DEBUG: %i purchases left.", mapACSales[iClientID].items.find(gbi.iGoodID)->second);
							wstring wscMsgLog = L"<%sender> has bought back <%item> from base <%basename> (%isale purchases left)";
							wscMsgLog = ReplaceStr(wscMsgLog, L"%sender", wscCharname.c_str());
							wscMsgLog = ReplaceStr(wscMsgLog, L"%basename", wscBaseName.c_str());
							wscMsgLog = ReplaceStr(wscMsgLog, L"%item", stows(iter->second).c_str());
							wscMsgLog = ReplaceStr(wscMsgLog, L"%isale", stows(itos(mapACSales[iClientID].items.find(gbi.iGoodID)->second))).c_str();
							string scText = wstos(wscMsgLog);
							Logging("%s", scText.c_str());

							if (mapACSales[iClientID].items.find(gbi.iGoodID)->second == 0)
							{
								mapACSales[iClientID].items.erase(gbi.iGoodID);
								//PrintUserCmdText(iClientID, L"DEBUG: no purchases left");
							}

							break;

						}

						if (wearecool == 0)
						{
							//PrintUserCmdText(iClientID, L"DEBUG: I have found this base, not good");
							aminiceitem = false;
							itemname = iter->second;
						}
					}
					i++;
				}
			}

			if (aminiceitem == false)
			{
				returncode = SKIPPLUGINS_NOFUNCTIONCALL;
				wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
				wstring wscBaseName = HkGetBaseNickByID(iBase);

				pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_anomaly_detected"));
				wstring wscMsgU = L"MF: %name has been permabanned. (Type 2)";
				wscMsgU = ReplaceStr(wscMsgU, L"%name", wscCharname.c_str());

				HkMsgU(wscMsgU);

				wstring wscMsgLog = L"<%sender> was permabanned for attempting to buy an illegal item <%item> from base <%basename> (see DSAM)";
				wscMsgLog = ReplaceStr(wscMsgLog, L"%sender", wscCharname.c_str());
				wscMsgLog = ReplaceStr(wscMsgLog, L"%basename", wscBaseName.c_str());
				wscMsgLog = ReplaceStr(wscMsgLog, L"%item", stows(itemname).c_str());

				LogCheater(iClientID, wscMsgLog);
			}
		}
	}
	else if (packageInfo->iType == 3)
	{
		uint iBase;
		pub::Player::GetBase(iClientID, iBase);
		//PrintUserCmdText(iClientID, L"This should be a ship purchase");
		bool aminiceship = false;

		for (map<uint, list<uint>>::iterator iter = mapACShips.begin(); iter != mapACShips.end(); iter++)
		{
			if (iter->first == iBase)
			{
				//PrintUserCmdText(iClientID, L"This should be a base");
				// we check if one of the three packages sold here is the correct one
				list<uint>::iterator i = iter->second.begin();
				while (i != iter->second.end())
				{
					if (*i == gbi.iGoodID)
					{
						//PrintUserCmdText(iClientID, L"I have found this ship");
						aminiceship = true;
						break;
					}
					i++;
				}
			}
		}

		if (aminiceship == false)
		{
			wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
			wstring wscBaseName = HkGetBaseNickByID(iBase);

			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_anomaly_detected"));
			wstring wscMsgU = L"MF: %name has been permabanned. (Type 1)";
			wscMsgU = ReplaceStr(wscMsgU, L"%name", wscCharname.c_str());

			HkMsgU(wscMsgU);

			wstring wscMsgLog = L"<%sender> was permabanned for attempting to buy an illegal ship from base <%basename> (see DSAM)";
			wscMsgLog = ReplaceStr(wscMsgLog, L"%sender", wscCharname.c_str());
			wscMsgLog = ReplaceStr(wscMsgLog, L"%basename", wscBaseName.c_str());

			LogCheater(iClientID, wscMsgLog);
		}
	}




}

void __stdcall BaseEnter_AFTER(unsigned int iBaseID, unsigned int iClientID)
{
	//PrintUserCmdText(iClientID, L"Cleared info");
	ClearClientInfo(iClientID);
}

void __stdcall PlayerLaunch(unsigned int iShip, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
	//PrintUserCmdText(client, L"Cleared info");
	ClearClientInfo(client);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions to hook
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "MarketFucker by Alley";
	p_PI->sShortName = "marketfucker";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ExecuteCommandString_Callback, PLUGIN_ExecuteCommandString_Callback, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&GFGoodBuy, PLUGIN_HkIServerImpl_GFGoodBuy, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&GFGoodSell, PLUGIN_HkIServerImpl_GFGoodSell, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch, PLUGIN_HkIServerImpl_PlayerLaunch, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ClearClientInfo, PLUGIN_ClearClientInfo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseEnter_AFTER, PLUGIN_HkIServerImpl_BaseEnter_AFTER, 0));

	return p_PI;
}
