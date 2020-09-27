// Events for FLHookPlugin
// December 2015 by BestDiscoveryHookDevs2015
//
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
#include <list>
#include <map>
#include <algorithm>
#include <FLHook.h>
#include <plugin.h>
#include <PluginUtilities.h>
#include "Main.h"
#include <sstream>
#include <iostream>
#include <hookext_exports.h>
#include "minijson_writer.hpp"
#include <set>

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

struct TRADE_EVENT {
	uint uHashID;
	string sEventName;
	string sURL;
	int iBonusCash;
	uint uStartBase;
	uint uEndBase;
	bool bFLHookBase = false;
	string sFLHookBaseName;
	int iObjectiveMax;
	int iObjectiveCurrent = 0; // Always 0 to prevent having no data
	uint uCommodityID;
	bool bLimited = false; //Whether or not this is limited to a specific set of IDs
	list<uint> lAllowedIDs;
};

struct COMBAT_EVENT {
	//Basic event settings
	string sEventName;
	string sURL;
	int iObjectiveMax;
	int iObjectiveCurrent = 0; // Always 0 to prevent having no data	
	//Combat event data
	bool bPlayersOnly = false; // assume false
	list<uint> lAllowedIDs;
	list<uint> lTargetIDs;
	list<uint> lSystems;
	//Rewards
	int bonusnpc;
	int bonusplayer;
	int iObjectivePlayerReward;
	int iObjectiveNPCReward;
	//NPC data if needed
	list<uint> lNPCTargetReputation;
	//Optional commodity reward data
	bool bCommodityReward = false; // assume false
	uint uCommodityID;
};

struct MINING_EVENT {
	//Basic event settings
	string sEventName;
	string sURL;
	int iObjectiveMax;
	int iObjectiveCurrent = 0; // Always 0 to prevent having no data
	int iBonusCash;
	//Mining settings
	bool bLimited = false; //Whether or not this is limited to a specific set of IDs
	set<uint> lAllowedMinerIDs;
	set<uint> lAllowedTraderIDs;
	uint uCommodityID;
	int iCommodityPerHit;
};

struct EVENT_TRACKER
{
	//wsccharname and amount of participation
	map<wstring, int> PlayerEventData;
	string eventname;
};

map<string, TRADE_EVENT> mapTradeEvents;
map<string, COMBAT_EVENT> mapCombatEvents;

map<string, EVENT_TRACKER> mapEventTracking;

map<uint, string> mapMiningSpaceObj;
map<string, MINING_EVENT> mapMiningEvents;

//gun projectile archs that are allowed to mine
set<uint> validminingarch;


//We'll map player IDs so we don't have to iterate through the player structures
//at some point this should be moved to HookExt so all plugins can benefit from this and reduce data redudancy.
map <uint, string> mapIDs;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Loading Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LoadIDs()
{
	string idfile = "..\\data\\equipment\\misc_equip.ini";
	int idcount = 0;

	INI_Reader ini;

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
		ConPrint(L"EVENT DEBUG: Loaded %u IDs\n", idcount);
	}
}

void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	string File_FLHook = "..\\exe\\flhook_plugins\\events.cfg";
	string File_FLHookStatus = "..\\exe\\flhook_plugins\\events_status.cfg";
	string File_FLHookTracker = "..\\exe\\flhook_plugins\\events_tracker.cfg";
	int iLoaded = 0;
	int iLoaded2 = 0;
	int iLoaded3 = 0;

	validminingarch.insert(CreateID("dsy_ecoturret_ammo"));
	validminingarch.insert(CreateID("dsy_miningturret_ammo"));

	INI_Reader ini;
	if (ini.open(File_FLHook.c_str(), false))
	{
		while (ini.read_header())
		{
			//Trade events
			if (ini.is_header("TradeEvent"))
			{
				TRADE_EVENT te;
				string id;

				while (ini.read_value())
				{
					if (ini.is_value("id"))
					{
						id = ini.get_value_string(0);
						te.uHashID = CreateID(ini.get_value_string(0));
					}
					else if (ini.is_value("name"))
					{
						te.sEventName = ini.get_value_string(0);
					}
					else if (ini.is_value("url"))
					{
						te.sURL = ini.get_value_string(0);
					}
					else if (ini.is_value("startbase"))
					{
						te.uStartBase = CreateID(ini.get_value_string(0));
					}
					else if (ini.is_value("endbase"))
					{
						te.uEndBase = CreateID(ini.get_value_string(0));
					}
					else if (ini.is_value("bonus"))
					{
						te.iBonusCash = ini.get_value_int(0);
					}
					else if (ini.is_value("objectivemax"))
					{
						te.iObjectiveMax = ini.get_value_int(0);
					}
					else if (ini.is_value("commodity"))
					{
						pub::GetGoodID(te.uCommodityID, ini.get_value_string(0));
					}
					else if (ini.is_value("limited"))
					{
						te.bLimited = ini.get_value_bool(0);
					}
					else if (ini.is_value("allowedid"))
					{
						te.lAllowedIDs.push_back(CreateID(ini.get_value_string(0)));
					}
					else if (ini.is_value("flhookbase"))
					{
						te.bFLHookBase = ini.get_value_bool(0);
					}
					else if (ini.is_value("flhookbasename"))
					{
						te.sFLHookBaseName = ini.get_value_string(0);
					}
				}

				mapTradeEvents[id] = te;
				++iLoaded;

			}
			//Combat Events
			else if (ini.is_header("CombatEvent"))
			{
				COMBAT_EVENT ce;
				string id;

				while (ini.read_value())
				{
					//Default event settings
					if (ini.is_value("id"))
					{
						id = ini.get_value_string(0);
					}
					else if (ini.is_value("name"))
					{
						ce.sEventName = ini.get_value_string(0);
					}
					else if (ini.is_value("url"))
					{
						ce.sURL = ini.get_value_string(0);
					}
					else if (ini.is_value("objectivemax"))
					{
						ce.iObjectiveMax = ini.get_value_int(0);
					}
					//Combat settings
					else if (ini.is_value("playersonly"))
					{
						ce.bPlayersOnly = ini.get_value_bool(0);
					}
					else if (ini.is_value("allowedid"))
					{
						ce.lAllowedIDs.push_back(CreateID(ini.get_value_string(0)));
					}
					else if (ini.is_value("targetid"))
					{
						ce.lTargetIDs.push_back(CreateID(ini.get_value_string(0)));
					}
					else if (ini.is_value("system"))
					{
						ce.lSystems.push_back(CreateID(ini.get_value_string(0)));
					}
					//Bonus values
					else if (ini.is_value("bonusnpc"))
					{
						ce.bonusnpc = ini.get_value_int(0);
						ce.iObjectiveNPCReward = ini.get_value_int(1);
					}
					else if (ini.is_value("bonusplayer"))
					{
						ce.bonusplayer = ini.get_value_int(0);
						ce.iObjectivePlayerReward = ini.get_value_int(1);
					}
					//NPC target reputations if enabled
					else if (ini.is_value("targetnpc"))
					{
						uint rep;
						pub::Reputation::GetReputationGroup(rep, ini.get_value_string(0));
						ce.lNPCTargetReputation.push_back(rep);
					}
					//Optional commodity reward
					else if (ini.is_value("commodityreward"))
					{
						ce.bCommodityReward = ini.get_value_bool(0);
					}
					else if (ini.is_value("commodity"))
					{
						pub::GetGoodID(ce.uCommodityID, ini.get_value_string(0));
					}
				}

				mapCombatEvents[id] = ce;
				++iLoaded;

			}
			//Mining Events
			else if (ini.is_header("MiningEvent"))
			{
				MINING_EVENT me;
				string id;

				while (ini.read_value())
				{
					//Default event settings
					if (ini.is_value("id"))
					{
						id = ini.get_value_string(0);
					}
					else if (ini.is_value("name"))
					{
						me.sEventName = ini.get_value_string(0);
					}
					else if (ini.is_value("url"))
					{
						me.sURL = ini.get_value_string(0);
					}
					else if (ini.is_value("objectivemax"))
					{
						me.iObjectiveMax = ini.get_value_int(0);
						me.iObjectiveCurrent = ini.get_value_int(0);
					}
					//Mining settings
					else if (ini.is_value("allowedminerid"))
					{
						me.lAllowedMinerIDs.insert(CreateID(ini.get_value_string(0)));
					}
					else if (ini.is_value("allowedtraderid"))
					{
						me.lAllowedTraderIDs.insert(CreateID(ini.get_value_string(0)));
					}
					//Bonus values
					else if (ini.is_value("bonus"))
					{
						me.iBonusCash = ini.get_value_int(0);
					}
					//Optional commodity reward
					else if (ini.is_value("limited"))
					{
						me.bLimited = ini.get_value_bool(0);
					}
					else if (ini.is_value("commodity"))
					{
						pub::GetGoodID(me.uCommodityID, ini.get_value_string(0));
					}
					else if (ini.is_value("commodityperhit"))
					{
						me.iCommodityPerHit = ini.get_value_int(0);
					}
				}

				mapMiningEvents[id] = me;
				++iLoaded;

			}

		}
		ini.close();
	}

	if (ini.open(File_FLHookStatus.c_str(), false))
	{
		while (ini.read_header())
		{
			//TRADE EVENTS
			if (ini.is_header("TradeEvent"))
			{
				bool exist = false;
				string id;
				int currentcount;

				while (ini.read_value())
				{
					if (ini.is_value("id"))
					{
						id = ini.get_value_string(0);
						//this is to ensure we don't keep data for events that ceased to exist
						if (mapTradeEvents.find(id) != mapTradeEvents.end())
						{
							exist = true;
						}
					}
					else if (ini.is_value("currentcount"))
					{
						currentcount = ini.get_value_int(0);
					}
				}

				if (exist)
				{
					mapTradeEvents[id].iObjectiveCurrent = currentcount;
					ConPrint(L"Event TE: Found event ID %s and loaded count %i\n", stows(id).c_str(), currentcount);
					++iLoaded2;
				}
			}
			//COMBAT EVENTS
			if (ini.is_header("CombatEvent"))
			{
				bool exist = false;
				string id;
				int currentcount;

				while (ini.read_value())
				{
					if (ini.is_value("id"))
					{
						id = ini.get_value_string(0);
						//this is to ensure we don't keep data for events that ceased to exist
						if (mapCombatEvents.find(id) != mapCombatEvents.end())
						{
							exist = true;
						}
					}
					else if (ini.is_value("currentcount"))
					{
						currentcount = ini.get_value_int(0);
					}
				}

				if (exist)
				{
					mapCombatEvents[id].iObjectiveCurrent = currentcount;
					ConPrint(L"Event CE: Found event ID %s and loaded count %i\n", stows(id).c_str(), currentcount);
					++iLoaded2;
				}
			}
			//COMBAT EVENTS
			if (ini.is_header("MiningEvent"))
			{
				bool exist = false;
				string id;
				int currentcount;

				while (ini.read_value())
				{
					if (ini.is_value("id"))
					{
						id = ini.get_value_string(0);
						//this is to ensure we don't keep data for events that ceased to exist
						if (mapMiningEvents.find(id) != mapMiningEvents.end())
						{
							exist = true;
						}
					}
					else if (ini.is_value("currentcount"))
					{
						currentcount = ini.get_value_int(0);
					}
				}

				if (exist)
				{
					mapMiningEvents[id].iObjectiveCurrent = currentcount;
					ConPrint(L"Event ME: Found event ID %s and loaded count %i\n", stows(id).c_str(), currentcount);
					++iLoaded2;
				}
			}

		}
		ini.close();
	}

	if (ini.open(File_FLHookTracker.c_str(), false))
	{
		while (ini.read_header())
		{
			//TRADE EVENTS
			if (ini.is_header("EventData"))
			{
				bool exist = false;

				string id;
				wstring wscCharname;
				int iCount;
				string name;

				while (ini.read_value())
				{
					if (ini.is_value("id"))
					{
						id = ini.get_value_string(0);
						//this is to ensure we don't keep data for events that ceased to exist
						if (mapTradeEvents.find(id) != mapTradeEvents.end())
						{
							exist = true;
							name = mapTradeEvents[id].sEventName;
						}
						else if (mapCombatEvents.find(id) != mapCombatEvents.end())
						{
							exist = true;
							name = mapCombatEvents[id].sEventName;
						}
						else if (mapMiningEvents.find(id) != mapMiningEvents.end())
						{
							exist = true;
							name = mapMiningEvents[id].sEventName;
						}
					}
					else if ((ini.is_value("data")) && (exist == true))
					{
						string delim = ", ";
						string data = ini.get_value_string();
						wscCharname = stows(data.substr(0, data.find(delim)));
						iCount = ToInt(data.substr(data.find(delim) + delim.length()));
						mapEventTracking[id].PlayerEventData[wscCharname] = iCount;
					}
				}

				if (exist)
				{
					mapEventTracking[id].eventname = name;
					++iLoaded3;
				}
			}
		}
		ini.close();
	}

	ConPrint(L"EVENT: Loaded %u events\n", iLoaded);
	ConPrint(L"EVENT DEBUG: Loaded %u event data\n", iLoaded2);
	ConPrint(L"EVENT DEBUG: Loaded %u event player data\n", iLoaded3);

	LoadIDs();

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

FILE *Logfile = fopen("./flhook_logs/event_log.log", "at");

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
	Logfile = fopen("./flhook_logs/event_log.log", "at");
}

void Notify_TradeEvent_Start(uint iClientID, string eventname)
{
	//internal log
	wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
	wstring wscMsgLog = L"<%player> has registered for the event <%eventname>";
	wscMsgLog = ReplaceStr(wscMsgLog, L"%player", wscCharname.c_str());
	wscMsgLog = ReplaceStr(wscMsgLog, L"%eventname", stows(eventname).c_str());
	string scText = wstos(wscMsgLog);
	Logging("%s", scText.c_str());
}

void Notify_TradeEvent_Exit(uint iClientID, string eventname, string reason)
{
	//internal log
	wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
	wstring wscMsgLog = L"<%player> has been unregistered from the event <%eventname>, reason: <%reason>";
	wscMsgLog = ReplaceStr(wscMsgLog, L"%player", wscCharname.c_str());
	wscMsgLog = ReplaceStr(wscMsgLog, L"%eventname", stows(eventname).c_str());
	wscMsgLog = ReplaceStr(wscMsgLog, L"%reason", stows(reason).c_str());
	string scText = wstos(wscMsgLog);
	Logging("%s", scText.c_str());
}

void Notify_TradeEvent_Completed(uint iClientID, string eventname, int iCargoCount, int iBonus)
{
	//internal log
	wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
	wstring wscMsgLog = L"<%player> has completed the event <%eventname> and delivered <%units> for a bonus of <%bonus> credits";
	wscMsgLog = ReplaceStr(wscMsgLog, L"%player", wscCharname.c_str());
	wscMsgLog = ReplaceStr(wscMsgLog, L"%eventname", stows(eventname).c_str());
	wscMsgLog = ReplaceStr(wscMsgLog, L"%units", stows(itos(iCargoCount)).c_str());
	wscMsgLog = ReplaceStr(wscMsgLog, L"%bonus", stows(itos(iBonus)).c_str());
	string scText = wstos(wscMsgLog);
	Logging("%s", scText.c_str());
}

void Notify_CombatEvent_PlayerKill(uint iClientIDKiller, uint iClientIDVictim, string eventname, int iCash, int iKillValue)
{
	//internal log
	wstring wscCharnameKiller = (const wchar_t*)Players.GetActiveCharacterName(iClientIDKiller);
	wstring wscCharnameVictim = (const wchar_t*)Players.GetActiveCharacterName(iClientIDVictim);
	wstring wscMsgLog = L"<%player> has killed <%victim> for the event <%eventname>, bonus: <%cash> worth <%points> points.";
	wscMsgLog = ReplaceStr(wscMsgLog, L"%player", wscCharnameKiller.c_str());
	wscMsgLog = ReplaceStr(wscMsgLog, L"%victim", wscCharnameVictim.c_str());
	wscMsgLog = ReplaceStr(wscMsgLog, L"%eventname", stows(eventname).c_str());
	wscMsgLog = ReplaceStr(wscMsgLog, L"%cash", stows(itos(iCash)).c_str());
	wscMsgLog = ReplaceStr(wscMsgLog, L"%points", stows(itos(iKillValue)).c_str());
	string scText = wstos(wscMsgLog);
	Logging("%s", scText.c_str());
}

void __stdcall CharacterSelect_AFTER(struct CHARACTER_ID const & cId, unsigned int iClientID)
{
	if (HookExt::IniGetB(iClientID, "event.enabled"))
	{
		string eventid = wstos(HookExt::IniGetWS(iClientID, "event.eventid"));

		//check if this event still exist
		if ((!empty(eventid)) && (mapTradeEvents.find(eventid) != mapTradeEvents.end()))
		{
			PrintUserCmdText(iClientID, L"You are still eligible to complete the event: %s", stows(mapTradeEvents[eventid].sEventName).c_str());
		}
		else
		{
			//else disable event mode
			HookExt::IniSetB(iClientID, "event.enabled", false);
			HookExt::IniSetWS(iClientID, "event.eventid", L"");
			HookExt::IniSetWS(iClientID, "event.eventpob", L"");
			HookExt::IniSetI(iClientID, "event.eventpobcommodity", 0);
			HookExt::IniSetI(iClientID, "event.quantity", 0);
			PrintUserCmdText(iClientID, L"You have been unregistered from an expired event.");
			Notify_TradeEvent_Exit(iClientID, eventid, "Logged in with expired event data");
		}
	}
}

void __stdcall GFGoodBuy_AFTER(struct SGFGoodBuyInfo const &gbi, unsigned int iClientID)
{
	for (map<string, TRADE_EVENT>::iterator i = mapTradeEvents.begin(); i != mapTradeEvents.end(); ++i)
	{
		//check if it's one of the commodities undergoing an event
		if (gbi.iGoodID == i->second.uCommodityID)
		{
			//this if is if we are interacting with this commodity and already in event mode
			if (HookExt::IniGetB(iClientID, "event.enabled"))
			{
				string eventid = wstos(HookExt::IniGetWS(iClientID, "event.eventid"));

				//leave event mode
				HookExt::IniSetB(iClientID, "event.enabled", false);
				HookExt::IniSetWS(iClientID, "event.eventid", L"");
				HookExt::IniSetWS(iClientID, "event.eventpob", L"");
				HookExt::IniSetI(iClientID, "event.eventpobcommodity", 0);
				HookExt::IniSetI(iClientID, "event.quantity", 0);
				PrintUserCmdText(iClientID, L"You have been unregistered from the event: %s", stows(mapTradeEvents[eventid].sEventName).c_str());
				Notify_TradeEvent_Exit(iClientID, i->second.sEventName, "Interacted with commodity on non-event station");
			}

			//this is as an if so if the player exited event mode he can reenter event mode
			//check if this is the event's stating point
			if ((gbi.iBaseID == i->second.uStartBase) && (!HookExt::IniGetB(iClientID, "event.enabled")))
			{
				if (i->second.bLimited)
				{
					uint pID = HookExt::IniGetI(iClientID, "event.shipid");
					bool bFoundID = false;

					for (list<uint>::iterator i2 = i->second.lAllowedIDs.begin(); i2 != i->second.lAllowedIDs.end(); ++i2)
					{
						if (*i2 == pID)
						{
							bFoundID = true;
						}
					}

					if (!bFoundID)
					{
						return;
					}
				}

				HookExt::IniSetB(iClientID, "event.enabled", true);
				HookExt::IniSetWS(iClientID, "event.eventid", stows(i->first));

				if (i->second.bFLHookBase == true)
				{
					HookExt::IniSetWS(iClientID, "event.eventpob", stows(i->second.sFLHookBaseName));
					HookExt::IniSetI(iClientID, "event.eventpobcommodity", i->second.uCommodityID);
				}
				else
				{
					HookExt::IniSetWS(iClientID, "event.eventpob", L"");
					HookExt::IniSetI(iClientID, "event.eventpobcommodity", 0);
				}

				HookExt::IniSetI(iClientID, "event.quantity", gbi.iCount);

				pub::Audio::PlaySoundEffect(iClientID, CreateID("ui_gain_level"));
				PrintUserCmdText(iClientID, L"You have entered the event: %s", stows(i->second.sEventName).c_str());
				Notify_TradeEvent_Start(iClientID, i->second.sEventName);

				return;
			}
		}
	}
}

void TradeEvent_Sale(struct SGFGoodSellInfo const &gsi, unsigned int iClientID)
{
	if (HookExt::IniGetB(iClientID, "event.enabled"))
	{
		for (map<string, TRADE_EVENT>::iterator i = mapTradeEvents.begin(); i != mapTradeEvents.end(); ++i)
		{
			//check if it's one of the commodities undergoing an event
			if (wstos(HookExt::IniGetWS(iClientID, "event.eventid")) == i->first)
			{
				//this if is if we are interacting with this commodity and already in event mode
				if (gsi.iArchID == i->second.uCommodityID)
				{
					uint iBaseID;
					pub::Player::GetBase(iClientID, iBaseID);

					//check if this is the event's end point
					if ((iBaseID == i->second.uEndBase))
					{
						int iInitialCount = HookExt::IniGetI(iClientID, "event.quantity");

						if (gsi.iCount > iInitialCount)
						{
							//leave event mode
							HookExt::IniSetB(iClientID, "event.enabled", false);
							HookExt::IniSetWS(iClientID, "event.eventid", L"");
							HookExt::IniSetWS(iClientID, "event.eventpob", L"");
							HookExt::IniSetI(iClientID, "event.eventpobcommodity", 0);
							HookExt::IniSetI(iClientID, "event.quantity", 0);
							PrintUserCmdText(iClientID, L"You have been unregistered from the event for having more cargo than you bought: %s", stows(i->second.sEventName).c_str());
							Notify_TradeEvent_Exit(iClientID, i->second.sEventName, "Delivered more cargo than bought at start point");
							return;
						}
						else
						{
							if (i->second.bFLHookBase)
							{
								//do nothing as FLHook base rewards are handled differently.
								continue;
							}

							HookExt::IniSetB(iClientID, "event.enabled", false);
							HookExt::IniSetWS(iClientID, "event.eventid", L"");
							HookExt::IniSetWS(iClientID, "event.eventpob", L"");
							HookExt::IniSetI(iClientID, "event.eventpobcommodity", 0);
							HookExt::IniSetI(iClientID, "event.quantity", 0);

							int bonus = 0;

							pub::Audio::PlaySoundEffect(iClientID, CreateID("ui_gain_level"));
							PrintUserCmdText(iClientID, L"You have finished the event: %s", stows(i->second.sEventName).c_str());

							wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);

							if (i->second.iObjectiveCurrent == i->second.iObjectiveMax)
							{
								PrintUserCmdText(iClientID, L"Sorry, this event is currently completed.");
								Notify_TradeEvent_Exit(iClientID, i->second.sEventName, "Completed trade run but event is completed");
								return;
							}
							else if ((i->second.iObjectiveCurrent + gsi.iCount) >= i->second.iObjectiveMax)
							{
								int amount = (i->second.iObjectiveCurrent + gsi.iCount) - i->second.iObjectiveMax;
								bonus = i->second.iBonusCash * amount;
								mapEventTracking[i->first].PlayerEventData[wscCharname] += amount;

								i->second.iObjectiveCurrent = i->second.iObjectiveMax;

								PrintUserCmdText(iClientID, L"You have completed the final delivery. Congratulations !");
								Notify_TradeEvent_Exit(iClientID, i->second.sEventName, "NOTIFICATION: Final Delivery");
							}
							else
							{
								bonus = i->second.iBonusCash * gsi.iCount;
								i->second.iObjectiveCurrent += gsi.iCount;
								mapEventTracking[i->first].PlayerEventData[wscCharname] += gsi.iCount;
							}




							HkAddCash(wscCharname, bonus);

							PrintUserCmdText(iClientID, L"You receive a bonus of: %d credits", bonus);
							Notify_TradeEvent_Completed(iClientID, i->second.sEventName, gsi.iCount, bonus);



						}
					}
					else if (!i->second.bFLHookBase)
					{
						//leave event mode
						HookExt::IniSetB(iClientID, "event.enabled", false);
						HookExt::IniSetWS(iClientID, "event.eventid", L"");
						HookExt::IniSetWS(iClientID, "event.eventpob", L"");
						HookExt::IniSetI(iClientID, "event.eventpobcommodity", 0);
						HookExt::IniSetI(iClientID, "event.quantity", 0);
						PrintUserCmdText(iClientID, L"You have been unregistered from the event: %s", stows(i->second.sEventName).c_str());
						Notify_TradeEvent_Exit(iClientID, i->second.sEventName, "Sold commodity to other base than delivery point");
					}
				}
			}
		}
	}
}

void __stdcall GFGoodSell_AFTER(struct SGFGoodSellInfo const &gsi, unsigned int iClientID)
{
	TradeEvent_Sale(gsi, iClientID);
	//MiningEvent_Sale(gsi, iClientID);
}

map<uint, uint> last_time_of_notice;
void __stdcall SPMunitionCollision(struct SSPMunitionCollisionInfo const & ci, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	if (mapMiningSpaceObj.size() == 0)
	{
		//PrintUserCmdText(iClientID, L"DEBUG: empty map, ignoring");
		return;
	}

	//check if we're hitting an eligible spaceobj
	map<uint, string>::iterator i = mapMiningSpaceObj.find(ci.dwTargetShip);

	if (i == mapMiningSpaceObj.end())
	{
		//PrintUserCmdText(iClientID, L"DEBUG: end of map, ignoring");
		return;
	}

	if (i != mapMiningSpaceObj.end())
	{
		MINING_EVENT eventdata = mapMiningEvents[i->second];
		//PrintUserCmdText(iClientID, stows(eventdata.sEventName).c_str());

		uint iSendToClientID = iClientID;

		if (validminingarch.find(ci.iProjectileArchID) == validminingarch.end())
		{
			PrintUserCmdText(iClientID, L"The rock is too tough. You need mining arrays for this.");
			return;
		}

		if (eventdata.iObjectiveCurrent == 0)
		{
			PrintUserCmdText(iClientID, L"Sorry, this event is completed.");
			return;
		}

		uint uPlayerID = HookExt::IniGetI(iClientID, "event.shipid");

		if (eventdata.bLimited)
		{
			if (eventdata.lAllowedMinerIDs.find(uPlayerID) == eventdata.lAllowedMinerIDs.end())
			{
				PrintUserCmdText(iClientID, L"Sorry, you can't participate in this event with this ID.");
				return;
			}
		}

		uint iShip = Players[iClientID].iShipID;
		uint iTargetShip;
		pub::SpaceObj::GetTarget(iShip, iTargetShip);
		if (iTargetShip)
		{
			uint iTargetClientID = HkGetClientIDByShip(iTargetShip);
			if (iTargetClientID)
			{
				if (HkDistance3DByShip(iShip, iTargetShip) < 1000.0f)
				{
					iSendToClientID = iTargetClientID;
				}
			}
		}

		int iLootCount = eventdata.iCommodityPerHit;
		uint iLootID = eventdata.uCommodityID;

		float fHoldRemaining;
		pub::Player::GetRemainingHoldSize(iSendToClientID, fHoldRemaining);
		if (fHoldRemaining < iLootCount)
		{
			iLootCount = (int)fHoldRemaining;
		}
		if (iLootCount == 0)
		{
			uint LastTime;
			if (last_time_of_notice.find(iClientID) == last_time_of_notice.end())
				LastTime = 0;
			else LastTime = last_time_of_notice[iClientID];

			if (((uint)time(0) - LastTime) > 1)
			{
				PrintUserCmdText(iClientID, L"%s's cargo is now full.", reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iSendToClientID)));
				pub::Player::SendNNMessage(iClientID, CreateID("insufficient_cargo_space"));
				if (iClientID != iSendToClientID)
				{
					PrintUserCmdText(iSendToClientID, L"Your cargo is now full.");
					pub::Player::SendNNMessage(iSendToClientID, CreateID("insufficient_cargo_space"));
				}
				last_time_of_notice[iClientID] = (uint)time(0);
			}
			return;
		}

		pub::Player::AddCargo(iSendToClientID, iLootID, iLootCount, 1.0, false);
		mapMiningEvents[i->second].iObjectiveCurrent -= iLootCount;
		wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
		mapEventTracking[i->second].PlayerEventData[wscCharname] += iLootCount;

		if (mapMiningEvents[i->second].iObjectiveCurrent < 0)
		{
			mapMiningEvents[i->second].iObjectiveCurrent = 0;
		}

		return;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Actual Code
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall PlayerLaunch_AFTER(struct CHARACTER_ID const & cId, unsigned int iClientID)
{
	for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
	{
		if ((mapIDs.find(item->iArchID) != mapIDs.end()) && item->bMounted)
		{
			HookExt::IniSetI(iClientID, "event.shipid", item->iArchID);
			//PrintUserCmdText(iClientID, L"DEBUG: Found ID %s", stows(mapIDs[item->iArchID]).c_str());
		}
	}
}

void ReadHookExtEventData()
{
	//Request the map
	map<uint, EVENT_PLUGIN_POB_TRANSFER> transfermap = HookExt::RequestPOBEventData();


	for (map<uint, EVENT_PLUGIN_POB_TRANSFER>::iterator iter = transfermap.begin(); iter != transfermap.end(); iter++)
	{
		//potentially useless check, but who knows
		if (HookExt::IniGetB(iter->first, "event.enabled"))
		{
			//necessary checks
			if (mapTradeEvents.find(iter->second.eventid) != mapTradeEvents.end())
			{
				//HkMsgU(L"EVENT POB DEBUG: Found event ID");

				if (mapTradeEvents[iter->second.eventid].iObjectiveCurrent == mapTradeEvents[iter->second.eventid].iObjectiveMax)
				{
					PrintUserCmdText(iter->first, L"Sorry, this event is currently completed.");
					Notify_TradeEvent_Exit(iter->first, mapTradeEvents[iter->second.eventid].sEventName, "Completed trade run but event is completed");
					continue;
				}
				else if ((mapTradeEvents[iter->second.eventid].iObjectiveCurrent + iter->second.count) >= mapTradeEvents[iter->second.eventid].iObjectiveMax)
				{
					mapTradeEvents[iter->second.eventid].iObjectiveCurrent = mapTradeEvents[iter->second.eventid].iObjectiveMax;
					PrintUserCmdText(iter->first, L"You have completed the final delivery. Congratulations !");
					Notify_TradeEvent_Exit(iter->first, mapTradeEvents[iter->second.eventid].sEventName, "NOTIFICATION: Final Delivery");
				}
				else
				{
					mapTradeEvents[iter->second.eventid].iObjectiveCurrent += iter->second.count;
				}

				int bonus = mapTradeEvents[iter->second.eventid].iBonusCash * iter->second.count;

				wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iter->first);
				HkAddCash(wscCharname, bonus);

				//leave event mode
				HookExt::IniSetB(iter->first, "event.enabled", false);
				HookExt::IniSetWS(iter->first, "event.eventid", L"");
				HookExt::IniSetWS(iter->first, "event.eventpob", L"");
				HookExt::IniSetI(iter->first, "event.eventpobcommodity", 0);
				HookExt::IniSetI(iter->first, "event.quantity", 0);

				PrintUserCmdText(iter->first, L"You have finished the event: %s", stows(mapTradeEvents[iter->second.eventid].sEventName).c_str());
				PrintUserCmdText(iter->first, L"You receive a bonus of: %d credits", bonus);
				Notify_TradeEvent_Completed(iter->first, mapTradeEvents[iter->second.eventid].sEventName, iter->second.count, bonus);
			}
		}
	}


}

void ProcessEventData()
{
	///////////////////////////////////////////////////////////////////////////////////////
	// JSON DUMPING INIT 
	///////////////////////////////////////////////////////////////////////////////////////

	stringstream stream;
	minijson::object_writer writer(stream);

	///////////////////////////////////////////////////////////////////////////////////////
	// INI DUMPING INIT
	///////////////////////////////////////////////////////////////////////////////////////

	//shamelessly stolen from the failed siege system
	string siegedump;

	//////////////////////////////////////////////////////////////////////////////////////
	// TRADE ITERATOR INIT
	///////////////////////////////////////////////////////////////////////////////////////

	for (map<string, TRADE_EVENT>::iterator iter = mapTradeEvents.begin(); iter != mapTradeEvents.end(); iter++)
	{
		///////////////////////////////////////////////////////////////////////////////////////
		// JSON DUMPING
		///////////////////////////////////////////////////////////////////////////////////////

		//begin the object writer
		minijson::object_writer pw = writer.nested_object(iter->first.c_str());

		//add basic elements
		pw.write("name", iter->second.sEventName);
		pw.write("url", iter->second.sURL);
		pw.write("current", iter->second.iObjectiveCurrent);
		pw.write("max", iter->second.iObjectiveMax);
		pw.close();

		///////////////////////////////////////////////////////////////////////////////////////
		// END JSON DUMPING
		///////////////////////////////////////////////////////////////////////////////////////

		///////////////////////////////////////////////////////////////////////////////////////
		// INI DUMPING
		///////////////////////////////////////////////////////////////////////////////////////

		string siegeblock;
		siegeblock = "[TradeEvent]\n";
		siegeblock.append("id = " + iter->first + "\n");

		stringstream ss;
		ss << iter->second.iObjectiveCurrent;
		string str = ss.str();

		siegeblock.append("currentcount = " + str + "\n");

		siegedump.append(siegeblock);

		///////////////////////////////////////////////////////////////////////////////////////
		// END INI DUMPING
		///////////////////////////////////////////////////////////////////////////////////////
	}

	//////////////////////////////////////////////////////////////////////////////////////
	// TRADE ITERATOR END
	///////////////////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////////////////
	// COMBAT ITERATOR INIT
	///////////////////////////////////////////////////////////////////////////////////////

	for (map<string, COMBAT_EVENT>::iterator iter = mapCombatEvents.begin(); iter != mapCombatEvents.end(); iter++)
	{
		///////////////////////////////////////////////////////////////////////////////////////
		// JSON DUMPING
		///////////////////////////////////////////////////////////////////////////////////////

		//begin the object writer
		minijson::object_writer pw = writer.nested_object(iter->first.c_str());

		//add basic elements
		pw.write("name", iter->second.sEventName);
		pw.write("url", iter->second.sURL);
		pw.write("current", iter->second.iObjectiveCurrent);
		pw.write("max", iter->second.iObjectiveMax);
		pw.close();

		///////////////////////////////////////////////////////////////////////////////////////
		// END JSON DUMPING
		///////////////////////////////////////////////////////////////////////////////////////

		///////////////////////////////////////////////////////////////////////////////////////
		// INI DUMPING
		///////////////////////////////////////////////////////////////////////////////////////

		string siegeblock;
		siegeblock = "[CombatEvent]\n";
		siegeblock.append("id = " + iter->first + "\n");

		stringstream ss;
		ss << iter->second.iObjectiveCurrent;
		string str = ss.str();

		siegeblock.append("currentcount = " + str + "\n");

		siegedump.append(siegeblock);

		///////////////////////////////////////////////////////////////////////////////////////
		// END INI DUMPING
		///////////////////////////////////////////////////////////////////////////////////////

	}

	//////////////////////////////////////////////////////////////////////////////////////
	// COMBAT ITERATOR END
	///////////////////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////////////////
	// MINING ITERATOR INIT
	///////////////////////////////////////////////////////////////////////////////////////

	for (map<string, MINING_EVENT>::iterator iter = mapMiningEvents.begin(); iter != mapMiningEvents.end(); iter++)
	{
		///////////////////////////////////////////////////////////////////////////////////////
		// JSON DUMPING
		///////////////////////////////////////////////////////////////////////////////////////

		//begin the object writer
		minijson::object_writer pw = writer.nested_object(iter->first.c_str());

		//add basic elements
		pw.write("name", iter->second.sEventName);
		pw.write("url", iter->second.sURL);
		pw.write("current", iter->second.iObjectiveCurrent);
		pw.write("max", iter->second.iObjectiveMax);
		pw.close();

		///////////////////////////////////////////////////////////////////////////////////////
		// END JSON DUMPING
		///////////////////////////////////////////////////////////////////////////////////////

		///////////////////////////////////////////////////////////////////////////////////////
		// INI DUMPING
		///////////////////////////////////////////////////////////////////////////////////////

		string siegeblock;
		siegeblock = "[MiningEvent]\n";
		siegeblock.append("id = " + iter->first + "\n");

		stringstream ss;
		ss << iter->second.iObjectiveCurrent;
		string str = ss.str();

		siegeblock.append("currentcount = " + str + "\n");

		siegedump.append(siegeblock);

		///////////////////////////////////////////////////////////////////////////////////////
		// END INI DUMPING
		///////////////////////////////////////////////////////////////////////////////////////

	}

	//////////////////////////////////////////////////////////////////////////////////////
	// COMBAT ITERATOR END
	///////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////
	// BEGIN JSON DUMPING
	///////////////////////////////////////////////////////////////////////////////////////

	writer.close();

	//dump to a file
	FILE *filejson = fopen("c:/stats/event_status.json", "w");
	if (filejson)
	{
		fprintf(filejson, stream.str().c_str());
		fclose(filejson);
	}
	///////////////////////////////////////////////////////////////////////////////////////
	// END JSON DUMPING
	///////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////
	// BEGIN INI DUMPING
	///////////////////////////////////////////////////////////////////////////////////////

	//dump to a file
	FILE *fileini = fopen("..\\exe\\flhook_plugins\\events_status.cfg", "w");
	if (fileini)
	{
		fprintf(fileini, siegedump.c_str());
		fclose(fileini);
	}

	///////////////////////////////////////////////////////////////////////////////////////
	// END INI DUMPING
	///////////////////////////////////////////////////////////////////////////////////////


}

void ProcessEventPlayerInfo()
{
	///////////////////////////////////////////////////////////////////////////////////////
	// JSON DUMPING INIT 
	///////////////////////////////////////////////////////////////////////////////////////

	stringstream stream;
	minijson::object_writer writer(stream);

	///////////////////////////////////////////////////////////////////////////////////////
	// INI DUMPING INIT
	///////////////////////////////////////////////////////////////////////////////////////

	//shamelessly stolen from the failed siege system
	string siegedump;

	//////////////////////////////////////////////////////////////////////////////////////
	// ITERATOR INIT
	///////////////////////////////////////////////////////////////////////////////////////

	for (map<string, EVENT_TRACKER>::iterator iter = mapEventTracking.begin(); iter != mapEventTracking.end(); iter++)
	{


		//begin the json object writer
		minijson::object_writer pw = writer.nested_object(iter->first.c_str());

		//begin the ini writer
		string siegeblock;
		siegeblock = "[EventData]\n";
		siegeblock.append("id = " + iter->first + "\n");

		for (map<wstring, int>::iterator i2 = iter->second.PlayerEventData.begin(); i2 != iter->second.PlayerEventData.end(); i2++)
		{
			pw.write(wstos(i2->first).c_str(), i2->second);

			stringstream ss;
			ss << i2->second;
			string str = ss.str();

			siegeblock.append("data = " + wstos(i2->first) + ", " + str + "\n");
		}

		pw.close();
		siegedump.append(siegeblock);

		///////////////////////////////////////////////////////////////////////////////////////
		// END INI DUMPING
		///////////////////////////////////////////////////////////////////////////////////////
	}

	//////////////////////////////////////////////////////////////////////////////////////
	// ITERATOR END
	///////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////
	// BEGIN JSON DUMPING
	///////////////////////////////////////////////////////////////////////////////////////

	writer.close();

	//dump to a file
	FILE *filejson = fopen("c:/stats/event_tracker.json", "w");
	if (filejson)
	{
		fprintf(filejson, stream.str().c_str());
		fclose(filejson);
	}
	///////////////////////////////////////////////////////////////////////////////////////
	// END JSON DUMPING
	///////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////
	// BEGIN INI DUMPING
	///////////////////////////////////////////////////////////////////////////////////////

	//dump to a file
	FILE *fileini = fopen("..\\exe\\flhook_plugins\\events_tracker.cfg", "w");
	if (fileini)
	{
		fprintf(fileini, siegedump.c_str());
		fclose(fileini);
	}

	///////////////////////////////////////////////////////////////////////////////////////
	// END INI DUMPING
	///////////////////////////////////////////////////////////////////////////////////////
}

void HkTimerCheckKick()
{
	returncode = DEFAULT_RETURNCODE;
	uint curr_time_events = (uint)time(0);

	if ((curr_time_events % 15) == 0)
	{
		ReadHookExtEventData();
	}

	if ((curr_time_events % 30) == 0)
	{
		ProcessEventData();
		ProcessEventPlayerInfo();

		map<uint, string> transfermap = HookExt::GetMiningEventObjs();
		mapMiningSpaceObj = transfermap;
	}

}


/// Hook for ship distruction. It's easier to hook this than the PlayerDeath one.
void SendDeathMsg(const wstring &wscMsg, uint iSystem, uint iClientIDVictim, uint iClientIDKiller)
{
	returncode = DEFAULT_RETURNCODE;

	const wchar_t *victim = (const wchar_t*)Players.GetActiveCharacterName(iClientIDVictim);
	const wchar_t *killer = (const wchar_t*)Players.GetActiveCharacterName(iClientIDKiller);

	string sIDVictimEvent;
	if (victim)
	{
		if (HookExt::IniGetB(iClientIDVictim, "event.enabled"))
		{
			sIDVictimEvent = wstos(HookExt::IniGetWS(iClientIDVictim, "event.eventid"));
			//else disable event mode
			HookExt::IniSetB(iClientIDVictim, "event.enabled", false);
			HookExt::IniSetWS(iClientIDVictim, "event.eventid", L"");
			HookExt::IniSetWS(iClientIDVictim, "event.eventpob", L"");
			HookExt::IniSetI(iClientIDVictim, "event.eventpobcommodity", 0);
			HookExt::IniSetI(iClientIDVictim, "event.quantity", 0);
			PrintUserCmdText(iClientIDVictim, L"You have died and have been unregistered from the event: %s", stows(mapTradeEvents[sIDVictimEvent].sEventName).c_str());
		}

	}

	if (victim && killer)
	{
		//Combat event handling for player death
		uint pIDKiller = HookExt::IniGetI(iClientIDKiller, "event.shipid");
		uint pIDVictim = HookExt::IniGetI(iClientIDVictim, "event.shipid");

		for (map<string, COMBAT_EVENT>::iterator i = mapCombatEvents.begin(); i != mapCombatEvents.end(); ++i)
		{
			//Check if this event has been completed already
			if (i->second.iObjectiveCurrent == i->second.iObjectiveMax)
			{
				PrintUserCmdText(iClientIDKiller, L"Sorry, the event is already completed.");
			}
			else
			{
				//Check if the kill was done in a system that match. This is most likely the fastest way to iterate through events at first.
				bool bFoundSystem = false;
				for (list<uint>::iterator i1 = i->second.lSystems.begin(); i1 != i->second.lSystems.end(); ++i1)
				{
					if (*i1 == iSystem)
					{
						bFoundSystem = true;
						break;
					}
				}

				if (bFoundSystem)
				{
					//Check first if our killer match the event
					bool bFoundIDKiller = false;
					for (list<uint>::iterator i2 = i->second.lAllowedIDs.begin(); i2 != i->second.lAllowedIDs.end(); ++i2)
					{
						if (*i2 == pIDKiller)
						{
							bFoundIDKiller = true;
							break;
						}
					}

					if (bFoundIDKiller)
					{
						//Check if our victim match the event
						bool bFoundIDVictim = false;
						for (list<uint>::iterator i3 = i->second.lTargetIDs.begin(); i3 != i->second.lTargetIDs.end(); ++i3)
						{
							if (*i3 == pIDVictim || *i3 == mapTradeEvents[sIDVictimEvent].uHashID)
							{
								bFoundIDVictim = true;
								break;
							}
						}

						if (bFoundIDVictim)
						{
							//If we reach this point we have a winner
							//Check event status first
							if ((i->second.iObjectiveCurrent + i->second.iObjectivePlayerReward) >= i->second.iObjectiveMax)
							{
								i->second.iObjectiveCurrent = i->second.iObjectiveMax;
								PrintUserCmdText(iClientIDKiller, L"You have delivered the final kill. Congratulations !");
								Notify_TradeEvent_Exit(iClientIDKiller, i->second.sEventName, "NOTIFICATION: Final Kill"); //must be changed
							}
							else
							{
								i->second.iObjectiveCurrent += i->second.iObjectivePlayerReward;
							}

							//Once we have updated the status, handle the reward
							//Provide commodity reward if chosen
							if (i->second.bCommodityReward == true)
							{
								//TODO
								break;
							}
							//Else provide money reward
							else
							{
								wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientIDKiller);
								HkAddCash(wscCharname, i->second.bonusplayer);

								mapEventTracking[i->first].PlayerEventData[wscCharname] += i->second.iObjectivePlayerReward;

								pub::Audio::PlaySoundEffect(iClientIDKiller, CreateID("ui_gain_level"));
								PrintUserCmdText(iClientIDKiller, L"You receive a bonus of %d credits and contributed %d points.", i->second.bonusplayer, i->second.iObjectivePlayerReward);
								Notify_CombatEvent_PlayerKill(iClientIDKiller, iClientIDVictim, i->second.sEventName, i->second.bonusplayer, i->second.iObjectivePlayerReward);
								break;
							}
						}
					}

				}
			}
		}
	}

}

/*
void __stdcall ShipDestroyed(DamageList *_dmg, DWORD *ecx, uint iKill)
{
	returncode = DEFAULT_RETURNCODE;
	//CShip *cship = (CShip*)ecx[4];

	//todo, combat event npc destruction
}
*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Client command processing
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
typedef bool(*_UserCmdProc)(uint, const wstring &, const wstring &, const wchar_t*);

struct USERCMD
{
	wchar_t *wszCmd;
	_UserCmdProc proc;
	wchar_t *usage;
};

USERCMD UserCmds[] =
{
	{ L"/autobuy", UserCmd_AutoBuy, L"Usage: /autobuy" },
	{ L"/autobuy*", UserCmd_AutoBuy, L"Usage: /autobuy" },
};
*/

/*
This function is called by FLHook when a user types a chat string. We look at the
string they've typed and see if it starts with one of the above commands. If it
does we try to process it.
*/

/*
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

*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions to hook
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "Events by Alley";
	p_PI->sShortName = "events";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkTimerCheckKick, PLUGIN_HkTimerCheckKick, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch_AFTER, PLUGIN_HkIServerImpl_PlayerLaunch_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterSelect_AFTER, PLUGIN_HkIServerImpl_CharacterSelect_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&GFGoodBuy_AFTER, PLUGIN_HkIServerImpl_GFGoodBuy_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&GFGoodSell_AFTER, PLUGIN_HkIServerImpl_GFGoodSell_AFTER, 0));
	//p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SendDeathMsg, PLUGIN_SendDeathMsg, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SPMunitionCollision, PLUGIN_HkIServerImpl_SPMunitionCollision, 0));

	return p_PI;
}
