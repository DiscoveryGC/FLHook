// CommodityLimit for FLHookPlugin
// February 2016 by Alley
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
#include <set>

#include "../hookext_plugin/hookext_exports.h"

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

bool bPluginEnabled = true;

struct CommodityLimitStruct
{
	list<wstring> TagRestrictions;
	list<uint> IDRestrictions;
	//TODO
	//list<uint> ShipClassRestrictions;
};

map<uint, CommodityLimitStruct> mapCommodityRestrictions;
map<uint, bool> mapBuySuppression;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Loading Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	string File_FLHook = "..\\exe\\flhook_plugins\\commodity_restrictions.cfg";
	int iLoaded = 0;
	int iLoaded2 = 0;

	INI_Reader ini;
	if (ini.open(File_FLHook.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("config"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("enabled"))
					{
						bPluginEnabled = ini.get_value_bool(0);
					}
				}
			}
			else if (ini.is_header("commodity"))
			{
				uint commodity;
				CommodityLimitStruct cls;
				while (ini.read_value())
				{
					if (ini.is_value("commodity"))
					{
						pub::GetGoodID(commodity, ini.get_value_string(0));
					}
					else if (ini.is_value("tag"))
					{
						cls.TagRestrictions.push_back(stows(ini.get_value_string(0)));
					}
					else if (ini.is_value("id"))
					{
						cls.IDRestrictions.push_back(CreateID(ini.get_value_string(0)));
					}
				}
				mapCommodityRestrictions[commodity] = cls;
				++iLoaded;
			}
		}
		ini.close();
	}

	ConPrint(L"CL: Loaded %u Limited Commodities\n", iLoaded);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Actual Code
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Clean up when a client disconnects */
void ClearClientInfo(uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	mapBuySuppression.erase(iClientID);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions to hook
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall GFGoodBuy(struct SGFGoodBuyInfo const &gbi, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	//Check if this a purchase this plugin must handle
	if (mapCommodityRestrictions.find(gbi.iGoodID) != mapCommodityRestrictions.end())
	{
		//Check to ensure this ship has been undocked at least once and the character has an hookext ID value stored
		uint pID = HookExt::IniGetI(iClientID, "event.shipid");
		if (pID != 0)
		{
			bool valid = false;
			//Check the ID to begin with, it's the most likely type of restriction
			if ((find(mapCommodityRestrictions[gbi.iGoodID].IDRestrictions.begin(), mapCommodityRestrictions[gbi.iGoodID].IDRestrictions.end(), pID) != mapCommodityRestrictions[gbi.iGoodID].IDRestrictions.end()))
			{
				//Allow the purchase
				valid = true;
			}
			else
			{
				//If the ID doesn't match, check for the tag
				wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
				for each (wstring tag in mapCommodityRestrictions[gbi.iGoodID].TagRestrictions)
				{
					if (wscCharname.find(tag) != string::npos)
					{
						valid = true;
						break;
					}
				}
			}

			//If none of the conditions have been met, deny the purchase
			if (!valid)
			{
				//deny the purchase
				returncode = SKIPPLUGINS_NOFUNCTIONCALL;
				PrintUserCmdText(iClientID, L"Sorry, you do not have permission to buy this item.");
				mapBuySuppression[iClientID] = true;
				return;
			}

		}
		else
		{
			//deny the purchase
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
			PrintUserCmdText(iClientID, L"Your ship is not initialized. Please undock once to initialize your server variables.");
			mapBuySuppression[iClientID] = true;
			return;
		}
	}
	//otherwise do nothing
}

/// Suppress the buying of goods.
void __stdcall ReqAddItem(unsigned int goodID, char const *hardpoint, int count, float status, bool mounted, uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	if (mapBuySuppression[iClientID])
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
	}
}

void __stdcall ReqChangeCash(int iMoneyDiff, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	if (mapBuySuppression[iClientID])
	{
		mapBuySuppression[iClientID] = false;
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
	}
}

EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "CommodityLimit by Alley";
	p_PI->sShortName = "commoditylimit";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ClearClientInfo, PLUGIN_ClearClientInfo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&GFGoodBuy, PLUGIN_HkIServerImpl_GFGoodBuy, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ReqAddItem, PLUGIN_HkIServerImpl_ReqAddItem, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ReqChangeCash, PLUGIN_HkIServerImpl_ReqChangeCash, 0));

	return p_PI;
}
