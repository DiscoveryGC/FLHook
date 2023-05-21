/**
 Base Plugin for FLHook-Plugin
 by Cannon.

0.1:
 Initial release
*/

// includes 

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
#include <hookext_exports.h>
#include <unordered_map>

// Clients
map<uint, CLIENT_DATA> clients;

// Bases
map<uint, PlayerBase*> player_bases;
map<uint, PlayerBase*>::iterator baseSaveIterator = player_bases.begin();

map<uint, bool> mapPOBShipPurchases;

/// 0 = HTML, 1 = JSON, 2 = Both
int ExportType = 0;

/// The debug mode
int set_plugin_debug = 0;

/// The ship used to construct and upgrade bases
uint set_construction_shiparch = 0;

/// Map of good to quantity for items required by construction ship
map<uint, uint> construction_items;

/// list of items and quantity used to repair 10000 units of damage
list<REPAIR_ITEM> set_base_repair_items;

/// list of items used by human crew
map<uint, uint> set_base_crew_consumption_items;
map<uint, uint> set_base_crew_food_items;

/// The commodity used as crew for the base
uint set_base_crew_type;

/// A return code to indicate to FLHook if we want the hook processing to continue.
PLUGIN_RETURNCODE returncode;

/// Map of item nickname hash to recipes to construct item.
map<uint, RECIPE> recipes;

/// Map of item nickname hash to recipes to operate shield.
map<uint, uint> shield_power_items;

/// Map of space obj IDs to base modules to speed up damage algorithms.
unordered_map<uint, Module*> spaceobj_modules;

/// Path to shield status html page
string set_status_path_html;

/// same thing but for json
string set_status_path_json;

/// Damage to the base every tick
uint set_damage_per_tick = 600;

/// Damage multiplier for damaged/abandoned stations
/// In case of overlapping modifiers, only the first one specified in .cfg file will apply
list<WEAR_N_TEAR_MODIFIER> wear_n_tear_mod_list;

/// Additional damage penalty for stations without proper crew
float no_crew_damage_multiplier = 1;

// The seconds per damage tick
uint set_damage_tick_time = 16;

// The seconds per tick
uint set_tick_time = 16;

// How much damage do we heal per repair cycle?
uint repair_per_repair_cycle = 60000;


// set of configurable variables defining the diminishing returns on damage during POB siege
// POB starts at base_shield_strength, then every 'threshold' of damage taken, 
// shield goes up in absorption by the 'increment'
// threshold size is to be configured per core level.
map<int, float> shield_reinforcement_threshold_map;
float shield_reinforcement_increment = 0.0f;
float base_shield_strength = 0.97f;

// decides if bases are globally immune, based on server time
bool isGlobalBaseInvulnerabilityActive;

list<BASE_VULNERABILITY_WINDOW> baseVulnerabilityWindows;

/// List of commodities forbidden to store on POBs
set<uint> forbidden_player_base_commodity_set;

// If true, use the new solar based defense platform spawn 	 	
bool set_new_spawn = true;

/// True if the settings should be reloaded
bool load_settings_required = true;

/// holiday mode
bool set_holiday_mode = false;

//pob sounds struct
POBSOUNDS pbsounds;

//archtype structure
map<string, ARCHTYPE_STRUCT> mapArchs;

//commodities to watch for logging
map<uint, wstring> listCommodities;

//the hostility and weapon platform activation from damage caused by one player
float damage_threshold = 400000;

//the amount of damage necessary to deal to one base in order to trigger siege status
float siege_mode_damage_trigger_level = 8000000;

//the distance between bases to share siege mod activation
float siege_mode_chain_reaction_trigger_distance = 8000;

uint GetAffliationFromClient(uint client)
{
	int rep;
	pub::Player::GetRep(client, rep);

	uint affiliation;
	Reputation::Vibe::Verify(rep);
	Reputation::Vibe::GetAffiliation(rep, affiliation, false);
	return affiliation;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

PlayerBase *GetPlayerBase(uint base)
{
	map<uint, PlayerBase*>::iterator i = player_bases.find(base);
	if (i != player_bases.end())
		return i->second;
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

PlayerBase *GetPlayerBaseForClient(uint client)
{
	map<uint, CLIENT_DATA>::iterator j = clients.find(client);
	if (j == clients.end())
		return 0;

	map<uint, PlayerBase*>::iterator i = player_bases.find(j->second.player_base);
	if (i == player_bases.end())
		return 0;

	return i->second;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

PlayerBase *GetLastPlayerBaseForClient(uint client)
{
	map<uint, CLIENT_DATA>::iterator j = clients.find(client);
	if (j == clients.end())
		return 0;

	map<uint, PlayerBase*>::iterator i = player_bases.find(j->second.last_player_base);
	if (i == player_bases.end())
		return 0;

	return i->second;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

	FILE *Logfile = fopen(("./flhook_logs/flhook_cheaters.log"), "at");
	if (Logfile)
	{
		fprintf(Logfile, "%s %s\n", szBuf, szBufString);
		fflush(Logfile);
		fclose(Logfile);
	}
}

// These logging functions need consolidating.
void BaseLogging(const char *szString, ...)
{
	char szBufString[1024];
	va_list marker;
	va_start(marker, szString);
	_vsnprintf(szBufString, sizeof(szBufString) - 1, szString, marker);

	char szBuf[64];
	time_t tNow = time(0);
	struct tm *t = localtime(&tNow);
	strftime(szBuf, sizeof(szBuf), "%d/%m/%Y %H:%M:%S", t);

	FILE *BaseLogfile = fopen("./flhook_logs/playerbase_events.log", "at");
	if (BaseLogfile)
	{
		fprintf(BaseLogfile, "%s %s\n", szBuf, szBufString);
		fflush(BaseLogfile);
		fclose(BaseLogfile);
	}
}

FILE *LogfileEventCommodities = fopen("./flhook_logs/event_pobsales.log", "at");

void LoggingEventCommodity(const char *szString, ...)
{
	char szBufString[1024];
	va_list marker;
	va_start(marker, szString);
	_vsnprintf(szBufString, sizeof(szBufString) - 1, szString, marker);

	char szBuf[64];
	time_t tNow = time(0);
	struct tm *t = localtime(&tNow);
	strftime(szBuf, sizeof(szBuf), "%d/%m/%Y %H:%M:%S", t);
	fprintf(LogfileEventCommodities, "%s %s\n", szBuf, szBufString);
	fflush(LogfileEventCommodities);
	fclose(LogfileEventCommodities);
	LogfileEventCommodities = fopen("./flhook_logs/event_pobsales.log", "at");
}

void Notify_Event_Commodity_Sold(uint iClientID, string commodity, int count, string basename)
{
	//internal log
	wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
	wstring wscMsgLog = L"<%player> has sold <%units> of the event commodity <%eventname> to the POB <%pob>";
	wscMsgLog = ReplaceStr(wscMsgLog, L"%player", wscCharname.c_str());
	wscMsgLog = ReplaceStr(wscMsgLog, L"%eventname", stows(commodity).c_str());
	wscMsgLog = ReplaceStr(wscMsgLog, L"%units", stows(itos(count)).c_str());
	wscMsgLog = ReplaceStr(wscMsgLog, L"%pob", stows(basename).c_str());
	string scText = wstos(wscMsgLog);
	LoggingEventCommodity("%s", scText.c_str());
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

	/*
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
	fprintf(file, "Autobanned by BasePlugin\n");
	fclose(file);
	}
	*/
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

// For the specified client setup the reputation to any bases in the
// client's system.
void SyncReputationForClientShip(uint ship, uint client)
{
	int player_rep;
	pub::SpaceObj::GetRep(ship, player_rep);

	uint system;
	pub::SpaceObj::GetSystem(ship, system);

	map<uint, PlayerBase*>::iterator base = player_bases.begin();
	for (; base != player_bases.end(); base++)
	{
		if (base->second->system == system)
		{
			float attitude = base->second->GetAttitudeTowardsClient(client);
			if (set_plugin_debug > 1)
				ConPrint(L"SyncReputationForClientShip:: ship=%u attitude=%f base=%08x\n", ship, attitude, base->first);
			for (vector<Module*>::iterator module = base->second->modules.begin();
				module != base->second->modules.end(); ++module)
			{
				if (*module)
				{
					(*module)->SetReputation(player_rep, attitude);
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// HTML-encodes a string and returns the encoded string.
wstring HtmlEncode(wstring text)
{
	wstring sb;
	int len = text.size();
	for (int i = 0; i < len; i++)
	{
		switch (text[i])
		{
		case L'<':
			sb.append(L"&lt;");
			break;
		case L'>':
			sb.append(L"&gt;");
			break;
		case L'"':
			sb.append(L"&quot;");
			break;
		case L'&':
			sb.append(L"&amp;");
			break;
		default:
			if (text[i] > 159)
			{
				sb.append(L"&#");
				sb.append(stows(itos((int)text[i])));
				sb.append(L";");
			}
			else
			{
				sb.append(1, text[i]);
			}
			break;
		}
	}
	return sb;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Clear client info when a client connects.
void ClearClientInfo(uint client)
{
	clients.erase(client);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;
	load_settings_required = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Load the configuration
void LoadSettingsActual()
{
	returncode = DEFAULT_RETURNCODE;

	// The path to the configuration file.
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string cfg_file = string(szCurDir) + "\\flhook_plugins\\base.cfg";
	string cfg_fileitems = string(szCurDir) + "\\flhook_plugins\\base_recipe_items.cfg";
	string cfg_filemodules = string(szCurDir) + "\\flhook_plugins\\base_recipe_modules.cfg";
	string cfg_filearch = string(szCurDir) + "\\flhook_plugins\\base_archtypes.cfg";
	string cfg_fileforbiddencommodities = string(szCurDir) + "\\flhook_plugins\\base_forbidden_cargo.cfg";

	map<uint, PlayerBase*>::iterator base = player_bases.begin();
	for (; base != player_bases.end(); base++)
	{
		delete base->second;
	}

	recipes.clear();
	construction_items.clear();
	set_base_repair_items.clear();
	set_base_crew_consumption_items.clear();
	set_base_crew_food_items.clear();
	shield_power_items.clear();

	HookExt::ClearMiningObjData();

	INI_Reader ini;
	if (ini.open(cfg_file.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("general"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("debug"))
					{
						set_plugin_debug = ini.get_value_int(0);
					}
					else if (ini.is_value("status_path_html"))
					{
						set_status_path_html = ini.get_value_string();
					}
					else if (ini.is_value("status_path_json"))
					{
						set_status_path_json = ini.get_value_string();
					}
					else if (ini.is_value("damage_threshold"))
					{
						damage_threshold = ini.get_value_float(0);
					}
					else if (ini.is_value("siege_mode_damage_trigger_level"))
					{
						siege_mode_damage_trigger_level = ini.get_value_float(0);
					}
					else if (ini.is_value("siege_mode_chain_reaction_trigger_distance"))
					{
						siege_mode_damage_trigger_level = ini.get_value_float(0);
					}
					else if (ini.is_value("status_export_type"))
					{
						ExportType = ini.get_value_int(0);
					}
					else if (ini.is_value("damage_per_tick"))
					{
						set_damage_per_tick = ini.get_value_int(0);
					}
					else if (ini.is_value("damage_multiplier"))
					{
						WEAR_N_TEAR_MODIFIER mod;
						mod.fromHP = ini.get_value_float(0);
						mod.toHP = ini.get_value_float(1);
						mod.modifier = ini.get_value_float(2);
						wear_n_tear_mod_list.push_back(mod);
					}
					else if (ini.is_value("no_crew_damage_multiplier"))
					{
						no_crew_damage_multiplier = ini.get_value_float(0);
					}
					else if (ini.is_value("damage_tick_time"))
					{
						set_damage_tick_time = ini.get_value_int(0);
					}
					else if (ini.is_value("tick_time"))
					{
						set_tick_time = ini.get_value_int(0);
					}
					else if (ini.is_value("health_to_heal_per_cycle"))
					{
						repair_per_repair_cycle = ini.get_value_int(0);
					}
					else if (ini.is_value("shield_reinforcement_threshold_per_core"))
					{
						shield_reinforcement_threshold_map.emplace(ini.get_value_int(0), ini.get_value_float(1));
					}
					else if (ini.is_value("shield_reinforcement_increment"))
					{
						shield_reinforcement_increment = ini.get_value_float(0);
					}
					else if (ini.is_value("base_shield_strength"))
					{
						base_shield_strength = ini.get_value_float(0);
					}
					else if (ini.is_value("base_vulnerability_window"))
					{
						BASE_VULNERABILITY_WINDOW damageWindow;
						damageWindow.start = ini.get_value_int(0);
						damageWindow.end = ini.get_value_int(1);
						baseVulnerabilityWindows.push_back(damageWindow);
					}
					else if (ini.is_value("construction_shiparch"))
					{
						set_construction_shiparch = CreateID(ini.get_value_string(0));
					}
					else if (ini.is_value("construction_item"))
					{
						uint good = CreateID(ini.get_value_string(0));
						uint quantity = ini.get_value_int(1);
						construction_items[good] = quantity;
					}
					else if (ini.is_value("base_crew_item"))
					{
						set_base_crew_type = CreateID(ini.get_value_string(0));
					}
					else if (ini.is_value("base_repair_item"))
					{
						REPAIR_ITEM item;
						item.good = CreateID(ini.get_value_string(0));
						item.quantity = ini.get_value_int(1);
						set_base_repair_items.push_back(item);
					}
					else if (ini.is_value("base_crew_consumption_item"))
					{
						uint good = CreateID(ini.get_value_string(0));
						uint quantity = ini.get_value_int(1);
						set_base_crew_consumption_items[good] = quantity;
					}
					else if (ini.is_value("base_crew_food_item"))
					{
						uint good = CreateID(ini.get_value_string(0));
						uint quantity = ini.get_value_int(1);
						set_base_crew_food_items[good] = quantity;
					}
					else if (ini.is_value("shield_power_item"))
					{
						uint good = CreateID(ini.get_value_string(0));
						uint quantity = ini.get_value_int(1);
						shield_power_items[good] = quantity;
					}
					else if (ini.is_value("set_new_spawn"))
					{
						set_new_spawn = true;
					}
					else if (ini.is_value("set_holiday_mode"))
					{
						set_holiday_mode = ini.get_value_bool(0);
						if (set_holiday_mode)
						{
							ConPrint(L"BASE: Attention, POB Holiday mode is enabled.\n");
						}
					}
					else if (ini.is_value("watch"))
					{
						uint c = CreateID(ini.get_value_string());
						listCommodities[c] = stows(ini.get_value_string());

					}
				}
			}
		}
		ini.close();
	}

	if (ini.open(cfg_fileitems.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("recipe"))
			{
				RECIPE recipe;
				while (ini.read_value())
				{
					if (ini.is_value("nickname"))
					{
						recipe.nickname = CreateID(ini.get_value_string(0));
					}
					else if (ini.is_value("produced_item"))
					{
						recipe.produced_item = CreateID(ini.get_value_string(0));
					}
					else if (ini.is_value("infotext"))
					{
						recipe.infotext = stows(ini.get_value_string());
					}
					else if (ini.is_value("cooking_rate"))
					{
						recipe.cooking_rate = ini.get_value_int(0);
					}
					else if (ini.is_value("consumed"))
					{
						recipe.consumed_items[CreateID(ini.get_value_string(0))] = ini.get_value_int(1);
					}
					else if (ini.is_value("reqlevel"))
					{
						recipe.reqlevel = ini.get_value_int(0);
					}
				}
				recipes[recipe.nickname] = recipe;
			}
		}
		ini.close();
	}

	if (ini.open(cfg_filemodules.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("recipe"))
			{
				RECIPE recipe;
				while (ini.read_value())
				{
					if (ini.is_value("nickname"))
					{
						recipe.nickname = CreateID(ini.get_value_string(0));
					}
					else if (ini.is_value("produced_item"))
					{
						recipe.produced_item = CreateID(ini.get_value_string(0));
					}
					else if (ini.is_value("infotext"))
					{
						recipe.infotext = stows(ini.get_value_string());
					}
					else if (ini.is_value("cooking_rate"))
					{
						recipe.cooking_rate = ini.get_value_int(0);
					}
					else if (ini.is_value("consumed"))
					{
						recipe.consumed_items[CreateID(ini.get_value_string(0))] = ini.get_value_int(1);
					}
					else if (ini.is_value("reqlevel"))
					{
						recipe.reqlevel = ini.get_value_int(0);
					}
				}
				recipes[recipe.nickname] = recipe;
			}
		}
		ini.close();
	}

	if (ini.open(cfg_filearch.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("arch"))
			{
				ARCHTYPE_STRUCT archstruct;
				string nickname = "default";
				int radius = 0;
				while (ini.read_value())
				{
					if (ini.is_value("nickname"))
					{
						nickname = ini.get_value_string(0);
					}
					else if (ini.is_value("invulnerable"))
					{
						archstruct.invulnerable = ini.get_value_int(0);
					}
					else if (ini.is_value("logic"))
					{
						archstruct.logic = ini.get_value_int(0);
					}
					else if (ini.is_value("radius"))
					{
						archstruct.radius = ini.get_value_float(0);
					}
					else if (ini.is_value("idrestriction"))
					{
						archstruct.idrestriction = ini.get_value_int(0);
					}
					else if (ini.is_value("isjump"))
					{
						archstruct.isjump = ini.get_value_int(0);
					}
					else if (ini.is_value("shipclassrestriction"))
					{
						archstruct.shipclassrestriction = ini.get_value_int(0);
					}
					else if (ini.is_value("allowedshipclasses"))
					{
						archstruct.allowedshipclasses.push_back(ini.get_value_int(0));
					}
					else if (ini.is_value("allowedids"))
					{
						archstruct.allowedids.push_back(CreateID(ini.get_value_string(0)));
					}
					else if (ini.is_value("module"))
					{
						archstruct.modules.push_back(ini.get_value_string(0));
					}
					else if (ini.is_value("display"))
					{
						archstruct.display = ini.get_value_bool(0);
					}
					else if (ini.is_value("mining"))
					{
						archstruct.mining = ini.get_value_bool(0);
					}
					else if (ini.is_value("miningevent"))
					{
						archstruct.miningevent = ini.get_value_string(0);
					}
				}
				mapArchs[nickname] = archstruct;
			}
		}
		ini.close();
	}

	if (ini.open(cfg_fileforbiddencommodities.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("forbidden_commodities"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("commodity_name"))
					{
						forbidden_player_base_commodity_set.insert(CreateID(ini.get_value_string(0)));
					}
				}
			}
		}
		ini.close();
	}

	//Create the POB sound hashes
	pbsounds.destruction1 = CreateID("pob_evacuate2");
	pbsounds.destruction2 = CreateID("pob_firecontrol");
	pbsounds.heavydamage1 = CreateID("pob_breach");
	pbsounds.heavydamage2 = CreateID("pob_reactor");
	pbsounds.heavydamage3 = CreateID("pob_heavydamage");
	pbsounds.mediumdamage1 = CreateID("pob_hullbreach");
	pbsounds.mediumdamage2 = CreateID("pob_critical");
	pbsounds.lowdamage1 = CreateID("pob_fire");
	pbsounds.lowdamage2 = CreateID("pob_engineering");

	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	// Create base account dir if it doesn't exist
	string basedir = string(datapath) + "\\Accts\\MultiPlayer\\player_bases\\";
	CreateDirectoryA(basedir.c_str(), 0);

	// Load and spawn all bases
	string path = string(datapath) + "\\Accts\\MultiPlayer\\player_bases\\base_*.ini";

	WIN32_FIND_DATA findfile;
	HANDLE h = FindFirstFile(path.c_str(), &findfile);
	if (h != INVALID_HANDLE_VALUE)
	{
		do
		{
			string filepath = string(datapath) + "\\Accts\\MultiPlayer\\player_bases\\" + findfile.cFileName;
			PlayerBase *base = new PlayerBase(filepath);
			player_bases[base->base] = base;
			base->Spawn();
		} while (FindNextFile(h, &findfile));
		FindClose(h);
	}

	// Load and sync player state
	struct PlayerData *pd = 0;
	while (pd = Players.traverse_active(pd))
	{
		uint client = pd->iOnlineID;
		if (HkIsInCharSelectMenu(client))
			continue;

		// If this player is in space, set the reputations.
		if (pd->iShipID)
			SyncReputationForClientShip(pd->iShipID, client);

		// Get state if player is in player base and  reset the commodity list
		// and send a dummy entry if there are no commodities in the market
		LoadDockState(client);
		if (clients[client].player_base)
		{
			PlayerBase *base = GetPlayerBaseForClient(client);
			if (base)
			{
				// Reset the commodity list	and send a dummy entry if there are no
				// commodities in the market
				SaveDockState(client);
				SendMarketGoodSync(base, client);
				SendBaseStatus(client, base);
			}
			else
			{
				// Force the ship to launch to space as the base has been destroyed
				DeleteDockState(client);
				SendResetMarketOverride(client);
				ForceLaunch(client);
			}
		}
	}
	PlayerCommands::Aff_initer();
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HkTimerCheckKick()
{
	returncode = DEFAULT_RETURNCODE;

	if (load_settings_required)
	{
		load_settings_required = false;
		LoadSettingsActual();
	}

	uint curr_time = (uint)time(0);
	isGlobalBaseInvulnerabilityActive = checkBaseVulnerabilityStatus();
	map<uint, PlayerBase*>::iterator iter = player_bases.begin();
	while (iter != player_bases.end())
	{
		PlayerBase *base = iter->second;
		// Advance to next base in case base is deleted in timer dispatcher
		++iter;
		// Dispatch timer but we can safely ignore the return
		base->Timer(curr_time);
	}
	if (!player_bases.empty() && !set_holiday_mode) {
		if (baseSaveIterator == player_bases.end()) {
			baseSaveIterator = player_bases.begin();
		}
		bool saveSuccessful = false;
		while (!saveSuccessful && baseSaveIterator != player_bases.end()) {
			auto& pb = baseSaveIterator->second;
			if (pb->logic == 1 || pb->invulnerable == 0) {
				pb->Save();
				saveSuccessful = true;
			}
			baseSaveIterator++;
		}
	}

	if (ExportType == 0 || ExportType == 2)
	{
		// Write status to an html formatted page every 60 seconds
		if ((curr_time % 60) == 0 && set_status_path_html.size() > 0)
		{
			ExportData::ToHTML();
		}
	}

	if (ExportType == 1 || ExportType == 2)
	{
		// Write status to a json formatted page every 60 seconds
		if ((curr_time % 60) == 0 && set_status_path_json.size() > 0)
		{
			ExportData::ToJSON();
		}
	}
}

bool __stdcall HkCb_IsDockableError(uint dock_with, uint base)
{
	if (GetPlayerBase(base))
		return false;
	ConPrint(L"ERROR: Base not found dock_with=%08x base=%08x\n", dock_with, base);
	return true;
}

__declspec(naked) void HkCb_IsDockableErrorNaked()
{
	__asm
	{
		test[esi + 0x1b4], eax
		jnz no_error
		push[edi + 0xB8]
		push[esi + 0x1b4]
		call HkCb_IsDockableError
		test al, al
		jz no_error
		push 0x62b76d3
		ret
		no_error :
		push 0x62b76fc
			ret
	}
}

bool __stdcall HkCb_Land(IObjInspectImpl *obj, uint base_dock_id, uint base)
{
	if (obj)
	{
		uint client = HkGetClientIDByShip(obj->get_id());
		if (client)
		{
			if (set_plugin_debug > 1)
				ConPrint(L"Land client=%u base_dock_id=%u base=%u\n", client, base_dock_id, base);

			// If we're docking at a player base, do nothing.
			if (clients[client].player_base)
				return true;

			// If we're not docking at a player base then clear 
			// the last base flag
			clients[client].last_player_base = 0;
			clients[client].player_base = 0;
			if (base == 0)
			{
				char szSystem[1024];
				pub::GetSystemNickname(szSystem, sizeof(szSystem), Players[client].iSystemID);

				char szProxyBase[1024];
				sprintf(szProxyBase, "%s_proxy_base", szSystem);

				uint iProxyBaseID = CreateID(szProxyBase);

				clients[client].player_base = base_dock_id;
				clients[client].last_player_base = base_dock_id;
				if (set_plugin_debug > 1)
					ConPrint(L"Land[2] client=%u baseDockID=%u base=%u player_base=%u\n", client, base_dock_id, base, clients[client].player_base);
				pub::Player::ForceLand(client, iProxyBaseID);
				return false;
			}
		}
	}
	return true;
}

__declspec(naked) void HkCb_LandNaked()
{
	__asm
	{
		mov al, [ebx + 0x1c]
		test al, al
		jz not_in_dock

		mov eax, [ebx + 0x18] // base id
		push eax
		mov eax, [esp + 0x14] // dock target
		push eax
		push edi // objinspect
		call HkCb_Land
		test al, al
		jz done

		not_in_dock :
		// Copied from moor.dll to support mooring.
		mov	al, [ebx + 0x1c]
			test	al, al
			jnz	done
			// It's false, so a safe bet that it's a moor.  Is it the player?
			mov	eax, [edi]
			mov	ecx, edi
			call[eax + 0xbc] // is_player
			test	al, al
			jnz done




			done :
		push 0x6D0C251
			ret
	}
}

static bool patched = false;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	srand((uint)time(0));
	// If we're being loaded from the command line while FLHook is running then
	// set_scCfgFile will not be empty so load the settings as FLHook only
	// calls load settings on FLHook startup and .rehash.
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		if (!patched)
		{
			patched = true;

			hModServer = GetModuleHandleA("server.dll");
			{

				// Call our function on landing
				byte patch[] = { 0xe9 }; // jmpr
				WriteProcMem((char*)hModServer + 0x2c24c, patch, sizeof(patch));
				PatchCallAddr((char*)hModServer, 0x2c24c, (char*)HkCb_LandNaked);
			}

			hModCommon = GetModuleHandleA("common.dll");
			{
				// Suppress "is dockable " error message
				byte patch[] = { 0xe9 }; // jmpr
				WriteProcMem((char*)hModCommon + 0x576cb, patch, sizeof(patch));
				PatchCallAddr((char*)hModCommon, 0x576cb, (char*)HkCb_IsDockableErrorNaked);
			}

			{
				// Suppress GetArch() error on max hit points call
				byte patch[] = { 0x90, 0x90 }; // nop nop
				WriteProcMem((char*)hModCommon + 0x995b6, patch, sizeof(patch));
				WriteProcMem((char*)hModCommon + 0x995fc, patch, sizeof(patch));
			}
		}

		HkLoadStringDLLs();
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
		if (patched)
		{
			{
				// Unpatch the landing hook
				byte patch[] = { 0x8A, 0x43, 0x1C, 0x84, 0xC0 };
				WriteProcMem((char*)hModServer + 0x2c24c, patch, sizeof(patch));
			}

			{
				// Unpatch the Suppress "is dockable " error message
				byte patch[] = { 0x85, 0x86, 0xb4, 0x01, 0x00 };
				WriteProcMem((char*)hModCommon + 0x576cb, patch, sizeof(patch));
			}
		}

		map<uint, PlayerBase*>::iterator base = player_bases.begin();
		for (; base != player_bases.end(); base++)
		{
			delete base->second;
		}

		HkUnloadStringDLLs();
	}
	return true;
}

bool UserCmd_Process(uint client, const wstring &args)
{
	returncode = DEFAULT_RETURNCODE;
	if (args.find(L"/base login") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseLogin(client, args);
		return true;
	}
	else if (args.find(L"/base addpwd") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseAddPwd(client, args);
		return true;
	}
	else if (args.find(L"/base rmpwd") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseRmPwd(client, args);
		return true;
	}
	else if (args.find(L"/base lstpwd") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseLstPwd(client, args);
		return true;
	}
	else if (args.find(L"/base setmasterpwd") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseSetMasterPwd(client, args);
		return true;
	}
	else if (args.find(L"/base addtag") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PrintUserCmdText(client, L"Checking if ship/tag exist in blacklist...");
		PlayerCommands::BaseRmHostileTag(client, args);
		PrintUserCmdText(client, L"Proceeding...");
		PlayerCommands::BaseAddAllyTag(client, args);
		return true;
	}
	else if (args.find(L"/base rmtag") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseRmAllyTag(client, args);
		return true;
	}
	else if (args.find(L"/base lsttag") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseLstAllyTag(client, args);
		return true;
	}
	else if (args.find(L"/base addfac") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseAddAllyFac(client, args);
		return true;
	}
	else if (args.find(L"/base rmfac") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseRmAllyFac(client, args);
		return true;
	}
	else if (args.find(L"/base clearfac") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseClearAllyFac(client, args);
		return true;
	}
	else if (args.find(L"/base lstfac") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseLstAllyFac(client, args);
		return true;
	}
	else if (args.find(L"/base addhfac") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseAddAllyFac(client, args, true);
		return true;
	}
	else if (args.find(L"/base rmhfac") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseRmAllyFac(client, args, true);
		return true;
	}
	else if (args.find(L"/base clearhfac") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseClearAllyFac(client, args, true);
		return true;
	}
	else if (args.find(L"/base lsthfac") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseLstAllyFac(client, args, true);
		return true;
	}
	else if (args.find(L"/base myfac") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseViewMyFac(client, args);
		return true;
	}
	else if (args.find(L"/base addhostile") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PrintUserCmdText(client, L"Checking if ship/tag exist in whitelist...");
		PlayerCommands::BaseRmAllyTag(client, args);
		PrintUserCmdText(client, L"Proceeding...");
		PlayerCommands::BaseAddHostileTag(client, args);
		return true;
	}
	else if (args.find(L"/base rmhostile") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseRmHostileTag(client, args);
		return true;
	}
	else if (args.find(L"/base lsthostile") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseLstHostileTag(client, args);
		return true;
	}
	else if (args.find(L"/base rep") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseRep(client, args);
		return true;
	}
	else if (args.find(L"/base defensemode") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseDefenseMode(client, args);
		return true;
	}
	else if (args.find(L"/base deploy") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseDeploy(client, args);
		return true;
	}
	else if (args.find(L"/shop") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::Shop(client, args);
		return true;
	}
	else if (args.find(L"/bank") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::Bank(client, args);
		return true;
	}
	else if (args.find(L"/base info") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseInfo(client, args);
		return true;
	}
	else if (args.find(L"/base supplies") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::GetNecessitiesStatus(client, args);
		return true;
	}
	else if (args.find(L"/base facmod") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseFacMod(client, args);
		return true;
	}
	else if (args.find(L"/base defmod") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseDefMod(client, args);
		return true;
	}
	else if (args.find(L"/base shieldmod") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseShieldMod(client, args);
		return true;
	}
	else if (args.find(L"/base buildmod") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseBuildMod(client, args);
		return true;
	}
	else if (args.find(L"/base") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		PlayerCommands::BaseHelp(client, args);
		return true;
	}
	return false;
}



static bool IsDockingAllowed(PlayerBase *base, uint client)
{
	// Allies can always dock.
	wstring charname = (const wchar_t*)Players.GetActiveCharacterName(client);
	for (list<wstring>::iterator i = base->ally_tags.begin(); i != base->ally_tags.end(); ++i)
	{
		if (charname.find(*i) == 0)
		{
			return true;
		}
	}

	//Hostile listed can't dock even if they are friendly faction listed
	for (list<wstring>::iterator i = base->perma_hostile_tags.begin(); i != base->perma_hostile_tags.end(); ++i)
	{
		if (charname.find(*i) == 0)
		{
			return false;
		}
	}

	uint playeraff = GetAffliationFromClient(client);
	//Do not allow dock if player is on the hostile faction list.
	if (base->hostile_factions.find(playeraff) != base->hostile_factions.end())
	{
		return false;
	}

	//Allow dock if player is on the friendly faction list.
	if (base->ally_factions.find(playeraff) != base->ally_factions.end())
	{
		return true;
	}

	// Base allows neutral ships to dock
	if (base->defense_mode == 2 && base->GetAttitudeTowardsClient(client) > -0.55f)
	{
		return true;
	}

	// Base allows neutral ships to dock
	if (base->defense_mode == 4)
	{
		return true;
	}

	return false;
}

// If this is a docking request at a player controlled based then send
// an update to set the base arrival text, base economy and change the
// infocards.

void __stdcall SystemSwitchOutComplete(unsigned int iShip, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	// Make player invincible to fix JHs/JGs near mine fields sometimes
	// exploding player while jumping (in jump tunnel)
	pub::SpaceObj::SetInvincible(iShip, true, true, 0);
	if (AP::SystemSwitchOutComplete(iShip, iClientID))
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
}

int __cdecl Dock_Call(unsigned int const &iShip, unsigned int const &base, int iCancel, enum DOCK_HOST_RESPONSE response)
{
	returncode = DEFAULT_RETURNCODE;

	uint client = HkGetClientIDByShip(iShip);
	//AP::ClearClientInfo(client);

	if (client && (response == PROCEED_DOCK || response == DOCK) && iCancel != -1)
	{
		PlayerBase* pbase = GetPlayerBase(base);
		if (pbase)
		{
			if (mapArchs[pbase->basetype].isjump == 1)
			{
				//check if we have an ID restriction
				if (mapArchs[pbase->basetype].idrestriction == 1)
				{
					bool foundid = false;
					for (list<EquipDesc>::iterator item = Players[client].equipDescList.equip.begin(); item != Players[client].equipDescList.equip.end(); item++)
					{
						if (item->bMounted)
						{
							list<uint>::iterator iditer = mapArchs[pbase->basetype].allowedids.begin();
							while (iditer != mapArchs[pbase->basetype].allowedids.end())
							{
								if (*iditer == item->iArchID)
								{
									foundid = true;
									PrintUserCmdText(client, L"DEBUG: Found acceptable ID.");
									break;
								}
								iditer++;
							}
						}
					}
					if (foundid == false)
					{
						PrintUserCmdText(client, L"ERR Unable to dock with this ID.");
						pub::Player::SendNNMessage(client, pub::GetNicknameId("info_access_denied"));
						returncode = SKIPPLUGINS_NOFUNCTIONCALL;
						return 0;
					}
				}

				//check if we have a shipclass restriction
				if (mapArchs[pbase->basetype].shipclassrestriction == 1)
				{
					bool foundclass = false;
					// get the player ship class
					Archetype::Ship* TheShipArch = Archetype::GetShip(Players[client].iShipArchetype);
					uint shipclass = TheShipArch->iShipClass;

					list<uint>::iterator iditer = mapArchs[pbase->basetype].allowedshipclasses.begin();
					while (iditer != mapArchs[pbase->basetype].allowedshipclasses.end())
					{
						if (*iditer == shipclass)
						{
							foundclass = true;
							PrintUserCmdText(client, L"DEBUG: Found acceptable shipclass.");
							break;
						}
						iditer++;
					}

					if (foundclass == false)
					{
						PrintUserCmdText(client, L"ERR Unable to dock with a vessel of this type.");
						pub::Player::SendNNMessage(client, pub::GetNicknameId("info_access_denied"));
						returncode = SKIPPLUGINS_NOFUNCTIONCALL;
						return 0;
					}
				}

				Vector pos;
				Matrix ornt;

				pub::SpaceObj::GetLocation(iShip, pos, ornt);

				pos.x = pbase->destposition.x;
				pos.y = pbase->destposition.y;
				pos.z = pbase->destposition.z;

				const Universe::ISystem *iSys = Universe::get_system(pbase->destsystem);
				wstring wscSysName = HkGetWStringFromIDS(iSys->strid_name);

				//PrintUserCmdText(client, L"Jump to system=%s x=%0.0f y=%0.0f z=%0.0f\n", wscSysName.c_str(), pos.x, pos.y, pos.z);
				AP::SwitchSystem(client, pbase->destsystem, pos, ornt);
				returncode = SKIPPLUGINS_NOFUNCTIONCALL;
				return 1;
			}

			// Shield is up, docking is not possible.
			if (pbase->shield_active_time)
			{
				PrintUserCmdText(client, L"Docking failed because base shield is active");
				pub::Player::SendNNMessage(client, pub::GetNicknameId("info_access_denied"));
				returncode = SKIPPLUGINS_NOFUNCTIONCALL;
				return 0;
			}

			if (!IsDockingAllowed(pbase, client))
			{
				PrintUserCmdText(client, L"Docking at this base is restricted");
				pub::Player::SendNNMessage(client, pub::GetNicknameId("info_access_denied"));
				returncode = SKIPPLUGINS_NOFUNCTIONCALL;
				return 0;
			}

			SendBaseStatus(client, pbase);
		}
	}
	return 0;
}

void __stdcall CharacterSelect(struct CHARACTER_ID const &cId, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;

	// Sync base names for the 
	map<uint, PlayerBase*>::iterator base = player_bases.begin();
	for (; base != player_bases.end(); base++)
	{
		HkChangeIDSString(client, base->second->solar_ids, base->second->basename);
	}
}

void __stdcall CharacterSelect_AFTER(struct CHARACTER_ID const &cId, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;

	if (set_plugin_debug > 1)
		ConPrint(L"CharacterSelect_AFTER client=%u player_base=%u\n", client, clients[client].player_base);

	// If this ship is in a player base is then set then docking ID to emulate
	// a landing.
	LoadDockState(client);
	if (clients[client].player_base)
	{
		if (set_plugin_debug > 1)
			ConPrint(L"CharacterSelect_AFTER[2] client=%u player_base=%u\n", client, clients[client].player_base);

		// If this base does not exist, dump the ship into space
		PlayerBase *base = GetPlayerBase(clients[client].player_base);
		if (!base)
		{
			DeleteDockState(client);
			SendResetMarketOverride(client);
			ForceLaunch(client);
		}
		// If the player file indicates that the ship is in a base but this isn't this
		// base then dump the ship into space.
		else if (Players[client].iBaseID != base->proxy_base)
		{
			DeleteDockState(client);
			SendResetMarketOverride(client);
			ForceLaunch(client);
		}
	}
}

void __stdcall BaseEnter(uint base, uint client)
{
	if (set_plugin_debug > 1)
		ConPrint(L"BaseEnter base=%u client=%u player_base=%u last_player_base=%u\n", base, client,
			clients[client].player_base, clients[client].last_player_base);

	returncode = DEFAULT_RETURNCODE;

	clients[client].admin = false;
	clients[client].viewshop = false;

	// If the last player base is set then we have not docked at a non player base yet.
	if (clients[client].last_player_base)
	{
		clients[client].player_base = clients[client].last_player_base;
	}

	// If the player is registered as being in a player controlled base then 
	// send the economy update, player system update and save a file to indicate
	// that we're in the base->
	if (clients[client].player_base)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (base)
		{
			// Reset the commodity list	and send a dummy entry if there are no
			// commodities in the market
			SaveDockState(client);
			SendMarketGoodSync(base, client);
			SendBaseStatus(client, base);
			return;
		}
		else
		{
			// Force the ship to launch to space as the base has been destroyed
			DeleteDockState(client);
			SendResetMarketOverride(client);
			ForceLaunch(client);
			return;
		}
	}

	DeleteDockState(client);
	SendResetMarketOverride(client);
}

void __stdcall BaseExit(uint base, uint client)
{
	returncode = DEFAULT_RETURNCODE;

	if (set_plugin_debug > 1)
		ConPrint(L"BaseExit base=%u client=%u player_base=%u\n", base, client, clients[client].player_base);

	// Reset client state and save it retaining the last player base ID to deal with respawn.
	clients[client].admin = false;
	clients[client].viewshop = false;
	if (clients[client].player_base)
	{
		if (set_plugin_debug)
			ConPrint(L"BaseExit base=%u client=%u player_base=%u\n", base, client, clients[client].player_base);

		clients[client].last_player_base = clients[client].player_base;
		clients[client].player_base = 0;
		SaveDockState(client);
	}
	else
	{
		DeleteDockState(client);
	}

	// Clear the base market and text
	SendResetMarketOverride(client);
	SendSetBaseInfoText2(client, L"");

	//wstring base_status = L"<RDL><PUSH/>";
	//base_status += L"<TEXT>" + XMLText(base->name) + L", " + HkGetWStringFromIDS(sys->strid_name) +  L"</TEXT><PARA/><PARA/>";
}

void __stdcall RequestEvent(int iIsFormationRequest, unsigned int iShip, unsigned int iDockTarget, unsigned int p4, unsigned long p5, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
	if (client)
	{
		if (!iIsFormationRequest)
		{
			PlayerBase *base = GetPlayerBase(iDockTarget);
			if (base)
			{
				// Shield is up, docking is not possible.
				if (base->shield_active_time)
				{
					PrintUserCmdText(client, L"Docking failed because base shield is active");
					pub::Player::SendNNMessage(client, pub::GetNicknameId("info_access_denied"));
					returncode = SKIPPLUGINS_NOFUNCTIONCALL;
					return;
				}

				if (!IsDockingAllowed(base, client))
				{
					PrintUserCmdText(client, L"Docking at this base is restricted");
					pub::Player::SendNNMessage(client, pub::GetNicknameId("info_access_denied"));
					returncode = SKIPPLUGINS_NOFUNCTIONCALL;
					return;
				}
			}
		}
	}
}

/// The base the player is launching from.
PlayerBase* player_launch_base = 0;

/// If the ship is launching from a player base record this so that
/// override the launch location.
bool __stdcall LaunchPosHook(uint space_obj, struct CEqObj &p1, Vector &pos, Matrix &rot, int dock_mode)
{
	returncode = DEFAULT_RETURNCODE;
	if (player_launch_base)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		pos = player_launch_base->position;
		rot = player_launch_base->rotation;
		TranslateX(pos, rot, -750);
		if (set_plugin_debug)
			ConPrint(L"LaunchPosHook[1] space_obj=%u pos=%0.0f %0.0f %0.0f dock_mode=%u\n",
				space_obj, pos.x, pos.y, pos.z, dock_mode);
		player_launch_base = 0;
	}
	return true;
}

/// If the ship is launching from a player base record this so that
/// we will override the launch location.
void __stdcall PlayerLaunch(unsigned int ship, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
	if (set_plugin_debug > 1)
		ConPrint(L"PlayerLaunch ship=%u client=%u\n", ship, client);
	player_launch_base = GetPlayerBase(clients[client].last_player_base);
}


void __stdcall PlayerLaunch_AFTER(unsigned int ship, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
	SyncReputationForClientShip(ship, client);
}

void __stdcall JumpInComplete(unsigned int system, unsigned int ship)
{
	returncode = DEFAULT_RETURNCODE;

	if (set_plugin_debug > 1)
		ConPrint(L"JumpInComplete system=%u ship=%u\n");

	uint client = HkGetClientIDByShip(ship);
	if (client)
	{
		SyncReputationForClientShip(ship, client);
	}
}

bool lastTransactionBase = false;
uint lastTransactionArchID = 0;
int lastTransactionCount = 0;
uint lastTransactionClientID = 0;

bool checkIfCommodityForbidden(uint goodId) {
	
	return forbidden_player_base_commodity_set.find(goodId) != forbidden_player_base_commodity_set.end();
}

void __stdcall GFGoodSell(struct SGFGoodSellInfo const &gsi, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
	lastTransactionBase = false;

	// If the client is in a player controlled base
	PlayerBase *base = GetPlayerBaseForClient(client);
	if (base)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		if (base->market_items.find(gsi.iArchID) == base->market_items.end()
			&& !clients[client].admin)
		{
			PrintUserCmdText(client, L"ERR: Base will not accept goods");
			clients[client].reverse_sell = true;
			return;
		}

		if (checkIfCommodityForbidden(gsi.iArchID)) {

			PrintUserCmdText(client, L"ERR: Cargo is not allowed on Player Bases");
			clients[client].reverse_sell = true;
			return;
		}

		MARKET_ITEM &item = base->market_items[gsi.iArchID];

		uint count = gsi.iCount;
		int price = (int)item.price * count;

		// base money check //
		if (count > ULONG_MAX / item.price)
		{
			clients[client].reverse_sell = true;
			PrintUserCmdText(client, L"KITTY ALERT. Illegal sale detected.");

			wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(client);
			pub::Player::SendNNMessage(client, pub::GetNicknameId("nnv_anomaly_detected"));
			wstring wscMsgU = L"KITTY ALERT: Possible type 3 POB cheating by %name (Count = %count, Price = %price)\n";
			wscMsgU = ReplaceStr(wscMsgU, L"%name", wscCharname.c_str());
			wscMsgU = ReplaceStr(wscMsgU, L"%count", stows(itos(count)).c_str());
			wscMsgU = ReplaceStr(wscMsgU, L"%price", stows(itos((int)item.price)).c_str());

			ConPrint(wscMsgU);
			LogCheater(client, wscMsgU);

			return;
		}

		if (price < 0)
		{
			clients[client].reverse_sell = true;
			PrintUserCmdText(client, L"KITTY ALERT. Illegal sale detected.");

			wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(client);
			pub::Player::SendNNMessage(client, pub::GetNicknameId("nnv_anomaly_detected"));
			wstring wscMsgU = L"KITTY ALERT: Possible type 4 POB cheating by %name (Count = %count, Price = %price)\n";
			wscMsgU = ReplaceStr(wscMsgU, L"%name", wscCharname.c_str());
			wscMsgU = ReplaceStr(wscMsgU, L"%count", stows(itos(count)).c_str());
			wscMsgU = ReplaceStr(wscMsgU, L"%price", stows(itos((int)item.price)).c_str());

			ConPrint(wscMsgU);
			LogCheater(client, wscMsgU);

			return;
		}

		// If the base doesn't have sufficient cash to support this purchase
		// reduce the amount purchased and shift the cargo back to the ship.
		if (base->money < price)
		{
			PrintUserCmdText(client, L"ERR: Base cannot accept goods, insufficient cash");
			clients[client].reverse_sell = true;
			return;
		}

		if ((item.quantity + count) > item.max_stock)
		{
			PrintUserCmdText(client, L"ERR: Base cannot accept goods, stock limit reached");
			clients[client].reverse_sell = true;
			return;
		}

		// Prevent player from getting invalid net worth.
		float fValue;
		pub::Player::GetAssetValue(client, fValue);

		int iCurrMoney;
		pub::Player::InspectCash(client, iCurrMoney);

		long long lNewMoney = iCurrMoney;
		lNewMoney += price;

		if (fValue + price > 2100000000 || lNewMoney > 2100000000)
		{
			PrintUserCmdText(client, L"ERR: Character too valuable.");
			clients[client].reverse_sell = true;
			return;
		}

		if (base->AddMarketGood(gsi.iArchID, gsi.iCount))
		{
			lastTransactionBase = true;
			lastTransactionArchID = gsi.iArchID;
			lastTransactionCount = gsi.iCount;
			lastTransactionClientID = client;
		}
		else
		{
			PrintUserCmdText(client, L"ERR: Base will not accept goods");
			clients[client].reverse_sell = true;
			return;
		}

		pub::Player::AdjustCash(client, price);
		base->ChangeMoney(0 - price);
		base->Save();

		if (listCommodities.find(gsi.iArchID) != listCommodities.end())
		{
			string cname = wstos(listCommodities[gsi.iArchID]);
			string cbase = wstos(base->basename);

			Notify_Event_Commodity_Sold(client, cname, gsi.iCount, cbase);
		}

		//build string and log the purchase
		wstring charname = (const wchar_t*)Players.GetActiveCharacterName(client);
		const GoodInfo *gi = GoodList_get()->find_by_id(gsi.iArchID);
		string gname = wstos(HtmlEncode(HkGetWStringFromIDS(gi->iIDSName)));
		string msg = "Player " + wstos(charname) + " sold item " + gname + " x" + itos(count);
		Log::LogBaseAction(wstos(base->basename), msg.c_str());

		//Event plugin hooks
		if (HookExt::IniGetB(client, "event.enabled") && (clients[client].reverse_sell == false))
		{
			//HkMsgU(L"DEBUG: POB event enabled");
			if (base->basename == HookExt::IniGetWS(client, "event.eventpob"))
			{
				//HkMsgU(L"DEBUG: event pob found");
				if (gsi.iArchID == HookExt::IniGetI(client, "event.eventpobcommodity"))
				{
					//HkMsgU(L"DEBUG: POB event commodity found");
					//At this point, send the data to HookExt
					PrintUserCmdText(client, L"Processing event deposit, please wait up to 15 seconds...");
					HookExt::AddPOBEventData(client, wstos(HookExt::IniGetWS(client, "event.eventid")), gsi.iCount);
				}
			}
		}


	}
}

void __stdcall ReqRemoveItem(unsigned short slot, int count, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;

	if (clients[client].player_base && clients[client].reverse_sell)
	{
		returncode = SKIPPLUGINS;
		int hold_size;
		HkEnumCargo((const wchar_t*)Players.GetActiveCharacterName(client), clients[client].cargo, hold_size);
	}
}

void __stdcall ReqRemoveItem_AFTER(unsigned short iID, int count, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;

	uint player_base = clients[client].player_base;
	if (player_base)
	{
		if (clients[client].reverse_sell)
		{
			returncode = SKIPPLUGINS;
			clients[client].reverse_sell = false;

			foreach(clients[client].cargo, CARGO_INFO, ci)
			{
				if (ci->iID == iID)
				{
					Server.ReqAddItem(ci->iArchID, ci->hardpoint.value, count, ci->fStatus, ci->bMounted, client);
					return;
				}
			}
		}
		else
		{
			// Update the player CRC so that the player is not kicked for 'ship related' kick
			PlayerData *pd = &Players[client];
			char *ACCalcCRC = (char*)hModServer + 0x6FAF0;
			__asm
			{
				pushad
				mov ecx, [pd]
				call[ACCalcCRC]
				mov ecx, [pd]
				mov[ecx + 320h], eax
				popad
			}
		}
	}
}

void __stdcall GFGoodBuy(struct SGFGoodBuyInfo const &gbi, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;

	// If the client is in a player controlled base
	PlayerBase *base = GetPlayerBaseForClient(client);
	if (base)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		uint count = gbi.iCount;
		if (count > base->market_items[gbi.iGoodID].quantity)
			count = base->market_items[gbi.iGoodID].quantity;

		int price = (int)base->market_items[gbi.iGoodID].price * count;
		int curr_money;
		pub::Player::InspectCash(client, curr_money);

		const wstring &charname = (const wchar_t*)Players.GetActiveCharacterName(client);

		// In theory, these should never be called.
		if (count == 0 || ((base->market_items[gbi.iGoodID].min_stock > (base->market_items[gbi.iGoodID].quantity - count)) && !clients[client].admin))
		{
			PrintUserCmdText(client, L"ERR Base will not sell goods");
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
			clients[client].stop_buy = true;
			return;
		}
		else if (curr_money < price)
		{
			PrintUserCmdText(client, L"ERR Not enough credits");
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
			clients[client].stop_buy = true;
			return;
		}

		if (((base->market_items[gbi.iGoodID].min_stock > (base->market_items[gbi.iGoodID].quantity - count)) && clients[client].admin))
			PrintUserCmdText(client, L"Permitted player-owned base good sale in violation of shop's minimum stock value due to base admin login.");

		clients[client].stop_buy = false;
		base->RemoveMarketGood(gbi.iGoodID, count);
		pub::Player::AdjustCash(client, 0 - price);
		base->ChangeMoney(price);
		base->Save();

		//build string and log the purchase
		const GoodInfo *gi = GoodList_get()->find_by_id(gbi.iGoodID);
		string gname = wstos(HtmlEncode(HkGetWStringFromIDS(gi->iIDSName)));
		string msg = "Player " + wstos(charname) + " purchased item " + gname + " x" + itos(count);
		Log::LogBaseAction(wstos(base->basename), msg.c_str());

		if (gi && gi->iType == GOODINFO_TYPE_SHIP)
		{
			returncode = SKIPPLUGINS;
			PrintUserCmdText(client, L"Purchased ship");
		}
		else if (gi && gi->iType == GOODINFO_TYPE_HULL)
		{
			returncode = SKIPPLUGINS;
			PrintUserCmdText(client, L"Purchased hull");
		}
	}
}

void __stdcall GFGoodBuy_AFTER(struct SGFGoodBuyInfo const &gbi, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	// If the client is in a player controlled base
	PlayerBase *base = GetPlayerBaseForClient(iClientID);
	if (base)
	{
		returncode = SKIPPLUGINS;
		// Update the player CRC so that the player is not kicked for 'ship related' kick
		PlayerData *pd = &Players[iClientID];
		char *ACCalcCRC = (char*)hModServer + 0x6FAF0;
		__asm
		{
			pushad
			mov ecx, [pd]
			call[ACCalcCRC]
			mov ecx, [pd]
			mov[ecx + 320h], eax
			popad
		}

		//PrintUserCmdText(iClientID, L"You will be kicked to update your ship.");
		//HkSaveChar((const wchar_t*)Players.GetActiveCharacterName(iClientID));
		//HkDelayedKick(iClientID, 10);

	}
}

void __stdcall ReqAddItem(unsigned int good, char const *hardpoint, int count, float fStatus, bool bMounted, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
	PlayerBase *base = GetPlayerBaseForClient(client);
	if (base)
	{
		returncode = SKIPPLUGINS;
		if (clients[client].stop_buy)
		{
			if (clients[client].stop_buy)
				clients[client].stop_buy = false;
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		}
	}
}

void __stdcall ReqAddItem_AFTER(unsigned int good, char const *hardpoint, int count, float fStatus, bool bMounted, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;

	// If the client is in a player controlled base
	PlayerBase *base = GetPlayerBaseForClient(client);
	if (base)
	{
		returncode = SKIPPLUGINS;
		PlayerData *pd = &Players[client];

		// Add to check-list which is being compared to the users equip-list when saving
		// char to fix "Ship or Equipment not sold on base" kick
		EquipDesc ed;
		ed.sID = pd->LastEquipID;
		ed.iCount = 1;
		ed.iArchID = good;
		pd->lShadowEquipDescList.add_equipment_item(ed, false);

		// Update the player CRC so that the player is not kicked for 'ship related' kick
		char *ACCalcCRC = (char*)hModServer + 0x6FAF0;
		__asm
		{
			pushad
			mov ecx, [pd]
			call[ACCalcCRC]
			mov ecx, [pd]
			mov[ecx + 320h], eax
			popad
		}
	}
}

/// Ignore cash commands from the client when we're in a player base.
void __stdcall ReqChangeCash(int cash, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
	if (clients[client].player_base)
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
}

/// Ignore cash commands from the client when we're in a player base.
void __stdcall ReqSetCash(int cash, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
	if (clients[client].player_base)
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
}


void __stdcall ReqEquipment(class EquipDescList const &edl, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
	if (clients[client].player_base)
		returncode = SKIPPLUGINS;
}

void __stdcall CShip_destroy(CShip* ship)
{
	returncode = DEFAULT_RETURNCODE;

	// Dispatch the destroy event to the appropriate module.
	uint space_obj = ship->get_id();
	auto& i = spaceobj_modules.find(space_obj);
	if (i != spaceobj_modules.end())
	{
		returncode = SKIPPLUGINS;
		i->second->SpaceObjDestroyed(space_obj);
	}
}

void BaseDestroyed(uint space_obj, uint client)
{
	returncode = DEFAULT_RETURNCODE;
	auto& i = spaceobj_modules.find(space_obj);
	if (i != spaceobj_modules.end())
	{
		returncode = SKIPPLUGINS;
		i->second->SpaceObjDestroyed(space_obj);
	}
}

void __stdcall HkCb_AddDmgEntry(DamageList *dmg, unsigned short sID, float& newHealth, enum DamageEntry::SubObjFate& fate)
{
	returncode = DEFAULT_RETURNCODE;
	if (!iDmgToSpaceID || !dmg->get_inflictor_id())
	{
		return;
	}
	
	if (!spaceobj_modules.count(iDmgToSpaceID)) {
		return;
	}

	Module* damagedModule = spaceobj_modules[iDmgToSpaceID];
	if(damagedModule->mining){
		return;
	}
	
	if (set_holiday_mode)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		iDmgToSpaceID = 0;
		return;
	}
	float curr, max;
	pub::SpaceObj::GetHealth(iDmgToSpaceID, curr, max);

	if (set_plugin_debug)
		ConPrint(L"HkCb_AddDmgEntry iDmgToSpaceID=%u get_inflictor_id=%u curr=%0.2f max=%0.0f newHealth=%0.2f cause=%u is_player=%u player_id=%u fate=%u\n",
			iDmgToSpaceID, dmg->get_inflictor_id(), curr, max, newHealth, dmg->get_cause(), dmg->is_inflictor_a_player(), dmg->get_inflictor_owner_player(), fate);

	// A work around for an apparent bug where mines/missiles at the base
	// causes the base damage to jump down to 0 even if the base is
	// otherwise healthy.
	if (newHealth == 0.0f /*&& dmg->get_cause()==7*/ && curr > 200000)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		if (set_plugin_debug)
			ConPrint(L"HkCb_AddDmgEntry[1] - invalid damage?\n");
		return;
	}

	// Ask the combat magic plugin if we need to do anything differently
	COMBAT_DAMAGE_OVERRIDE_STRUCT info;
	info.iMunitionID = iDmgMunitionID;
	info.fDamageMultiplier = 0.0f;
	Plugin_Communication(COMBAT_DAMAGE_OVERRIDE, &info);

	if (info.fDamageMultiplier != 0.0f)
	{
		newHealth = (curr - (curr - newHealth) * info.fDamageMultiplier);
		if (newHealth < 0.0f)
			newHealth = 0.0f;
	}

	// This call is for us, skip all plugins.
	iDmgToSpaceID = 0;
	newHealth = damagedModule->SpaceObjDamaged(iDmgToSpaceID, dmg->get_inflictor_id(), curr, newHealth);
	if (newHealth == curr) {
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		return;
	}

	returncode = SKIPPLUGINS;

	if (newHealth <= 0 && sID == 1)
	{
		uint iType;
		pub::SpaceObj::GetType(iDmgToSpaceID, iType);
		uint iClientIDKiller = HkGetClientIDByShip(dmg->get_inflictor_id());
		if (set_plugin_debug)
			ConPrint(L"HkCb_AddDmgEntry[2]: iType is %u, iClientIDKiller is %u\n", iType, iClientIDKiller);
		if (iClientIDKiller && iType & (OBJ_DOCKING_RING | OBJ_STATION | OBJ_WEAPONS_PLATFORM))
			BaseDestroyed(iDmgToSpaceID, iClientIDKiller);
	}
}

static void ForcePlayerBaseDock(uint client, PlayerBase *base)
{
	char system_nick[1024];
	pub::GetSystemNickname(system_nick, sizeof(system_nick), base->system);

	char proxy_base_nick[1024];
	sprintf(proxy_base_nick, "%s_proxy_base", system_nick);

	uint proxy_base_id = CreateID(proxy_base_nick);

	clients[client].player_base = base->base;
	clients[client].last_player_base = base->base;

	if (set_plugin_debug > 1)
		ConPrint(L"ForcePlayerBaseDock client=%u player_base=%u\n", client, clients[client].player_base);

	uint system;
	pub::Player::GetSystem(client, system);

	pub::Player::ForceLand(client, proxy_base_id);
	if (system != base->system)
	{
		Server.BaseEnter(proxy_base_id, client);
		Server.BaseExit(proxy_base_id, client);

		wstring charname = (const wchar_t*)Players.GetActiveCharacterName(client);
		wstring charfilename;
		HkGetCharFileName(charname, charfilename);
		charfilename += L".fl";
		CHARACTER_ID charid;
		strcpy(charid.szCharFilename, wstos(charname.substr(0, 14)).c_str());

		Server.CharacterSelect(charid, client); \
	}
}

#define IS_CMD(a) !args.compare(L##a)
#define RIGHT_CHECK(a) if(!(cmd->rights & a)) { cmd->Print(L"ERR No permission\n"); return true; }
bool ExecuteCommandString_Callback(CCmds* cmd, const wstring &args)
{
	returncode = DEFAULT_RETURNCODE;
	/*if (args.find(L"dumpbases")==0)
	{
		Universe::ISystem *sys = Universe::GetFirstSystem();
		FILE* f = fopen("bases.txt", "w");
		while (sys)
		{
			fprintf(f, "[Base]\n");
			fprintf(f, "nickname = %s_proxy_base\n", sys->nickname);
			fprintf(f, "system = %s\n", sys->nickname);
			fprintf(f, "strid_name = 0\n");
			fprintf(f, "file=Universe\\Systems\\proxy_base->ini\n");
			fprintf(f, "BGCS_base_run_by=W02bF35\n\n");

			sys = Universe::GetNextSystem();
		}
		fclose(f);
	}
	if (args.find(L"makebases")==0)
	{
		struct Universe::ISystem *sys = Universe::GetFirstSystem();
		while (sys)
		{
			string path = string("..\\DATA\\UNIVERSE\\SYSTEMS\\") + string(sys->nickname) + "\\" + string(sys->nickname) + ".ini";
			FILE *file = fopen(path.c_str(), "a+");
			if (file)
			{
				ConPrint(L"doing path %s\n", stows(path).c_str());
				fprintf(file, "\n\n[Object]\n");
				fprintf(file, "nickname = %s_proxy_base\n", sys->nickname);
				fprintf(file, "dock_with = %s_proxy_base\n", sys->nickname);
				fprintf(file, "base = %s_proxy_base\n", sys->nickname);
				fprintf(file, "pos = 0, -100000, 0\n");
				fprintf(file, "archetype = invisible_base\n");
				fprintf(file, "behavior = NOTHING\n");
				fprintf(file, "visit = 128\n");
				fclose(file);
			}
			sys = Universe::GetNextSystem();
		}
		return true;
	}*/

	if (args.find(L"testrecipe") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		RIGHT_CHECK(RIGHT_BASES)

		uint client = HkGetClientIdFromCharname(cmd->GetAdminName());
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			cmd->Print(L"ERR Not in player base");
			return true;
		}

		uint recipe_name = CreateID(wstos(cmd->ArgStr(1)).c_str());

		RECIPE recipe = recipes[recipe_name];
		for (map<uint, uint>::iterator i = recipe.consumed_items.begin(); i != recipe.consumed_items.end(); ++i)
		{
			base->market_items[i->first].quantity += i->second;
			SendMarketGoodUpdated(base, i->first, base->market_items[i->first]);
			cmd->Print(L"Added %ux %08x", i->second, i->first);
		}
		base->Save();
		cmd->Print(L"OK");
		return true;
	}
	else if (args.find(L"testdeploy") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		RIGHT_CHECK(RIGHT_BASES)

		uint client = HkGetClientIdFromCharname(cmd->GetAdminName());
		if (!client)
		{
			cmd->Print(L"ERR Not in game");
			return true;
		}

		for (map<uint, uint>::iterator i = construction_items.begin(); i != construction_items.end(); ++i)
		{
			uint good = i->first;
			uint quantity = i->second;
			pub::Player::AddCargo(client, good, quantity, 1.0, false);
		}

		cmd->Print(L"OK");
		return true;
	}
	else if (args.compare(L"beam") == 0)
	{
		returncode = DEFAULT_RETURNCODE;
		wstring charname = cmd->ArgCharname(1);
		wstring basename = cmd->ArgStrToEnd(2);

		// Fall back to default behaviour.
		if (cmd->rights != RIGHT_SUPERADMIN)
		{
			return false;
		}

		HKPLAYERINFO info;
		if (HkGetPlayerInfo(charname, info, false) != HKE_OK)
		{
			return false;
		}

		if (info.iShip == 0)
		{
			return false;
		}

		// Search for an match at the start of the name
		for (map<uint, PlayerBase*>::iterator i = player_bases.begin(); i != player_bases.end(); ++i)
		{
			if (ToLower(i->second->basename).find(ToLower(basename)) == 0)
			{
				returncode = SKIPPLUGINS_NOFUNCTIONCALL;
				ForcePlayerBaseDock(info.iClientID, i->second);
				cmd->Print(L"OK");
				return true;
			}
		}

		// Exact match failed, try a for an partial match
		for (map<uint, PlayerBase*>::iterator i = player_bases.begin(); i != player_bases.end(); ++i)
		{
			if (ToLower(i->second->basename).find(ToLower(basename)) != -1)
			{
				returncode = SKIPPLUGINS_NOFUNCTIONCALL;
				ForcePlayerBaseDock(info.iClientID, i->second);
				cmd->Print(L"OK");
				return true;
			}
		}

		// Fall back to default flhook .beam command
		return false;
	}
	else if (args.find(L"basedestroy") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		RIGHT_CHECK(RIGHT_BASES)

		uint client = HkGetClientIdFromCharname(cmd->GetAdminName());

		//return SpaceObjDestroyed(space_obj);
		//alleynote1
		int billythecat = 0;
		PlayerBase *base;
		for (map<uint, PlayerBase*>::iterator i = player_bases.begin(); i != player_bases.end(); ++i)
		{
			if (i->second->basename == cmd->ArgStrToEnd(1))
			{
				base = i->second;
				billythecat = 1;
			}
		}


		if (billythecat == 0)
		{
			cmd->Print(L"ERR Base doesn't exist lmao");
			return true;
		}

		base->base_health = 0;
		if (base->base_health < 1)
		{
			return CoreModule(base).SpaceObjDestroyed(CoreModule(base).space_obj);
		}

		//cmd->Print(L"OK Base is gone are you proud of yourself.");
		return true;
	}
	else if (args.find(L"basetogglegod") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		RIGHT_CHECK(RIGHT_BASES)

		uint client = HkGetClientIdFromCharname(cmd->GetAdminName());
		bool optype = cmd->ArgInt(1);

		//return SpaceObjDestroyed(space_obj);
		//alleynote1
		int billythecat = 0;
		PlayerBase *base;
		for (map<uint, PlayerBase*>::iterator i = player_bases.begin(); i != player_bases.end(); ++i)
		{
			if (i->second->basename == cmd->ArgStrToEnd(2))
			{
				base = i->second;
				billythecat = 1;
				break;
			}
		}


		if (billythecat == 0)
		{
			cmd->Print(L"ERR Base doesn't exist lmao");
			return true;
		}


		if (optype == true)
		{
			base->invulnerable = true;
			cmd->Print(L"OK Base made invulnerable.");
		}
		else if (optype == false)
		{
			base->invulnerable = false;
			cmd->Print(L"OK Base made vulnerable.");
		}

		//cmd->Print(L"OK Base is gone are you proud of yourself.");
		return true;
	}
	else if (args.find(L"testbase") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		RIGHT_CHECK(RIGHT_BASES)

		uint client = HkGetClientIdFromCharname(cmd->GetAdminName());

		uint ship;
		pub::Player::GetShip(client, ship);
		if (!ship)
		{
			PrintUserCmdText(client, L"ERR Not in space");
			return true;
		}

		int min = 100;
		int max = 5000;
		int randomsiegeint = min + (rand() % (int)(max - min + 1));

		string randomname = "TB";

		stringstream ss;
		ss << randomsiegeint;
		string str = ss.str();

		randomname.append(str);

		// Check for conflicting base name
		if (GetPlayerBase(CreateID(PlayerBase::CreateBaseNickname(randomname).c_str())))
		{
			PrintUserCmdText(client, L"ERR Deployment error, please reiterate.");
			return true;
		}

		wstring charname = (const wchar_t*)Players.GetActiveCharacterName(client);
		AddLog("NOTICE: Base created %s by %s (%s)",
			randomname.c_str(),
			wstos(charname).c_str(),
			wstos(HkGetAccountID(HkGetAccountByCharname(charname))).c_str());

		wstring password = L"hastesucks";
		wstring basename = stows(randomname);

		PlayerBase *newbase = new PlayerBase(client, password, basename);
		player_bases[newbase->base] = newbase;
		newbase->basetype = "legacy";
		newbase->basesolar = "legacy";
		newbase->baseloadout = "legacy";
		newbase->defense_mode = 1;

		for (map<string, ARCHTYPE_STRUCT>::iterator iter = mapArchs.begin(); iter != mapArchs.end(); iter++)
		{

			ARCHTYPE_STRUCT &thearch = iter->second;
			if (iter->first == newbase->basetype)
			{
				newbase->invulnerable = thearch.invulnerable;
				newbase->logic = thearch.logic;
			}
		}

		newbase->Spawn();
		newbase->Save();

		PrintUserCmdText(client, L"OK: Siege Cannon deployed");
		PrintUserCmdText(client, L"Default administration password is %s", password.c_str());

		return true;
	}
	else if (args.find(L"jumpcreate") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		RIGHT_CHECK(RIGHT_BASES)

		uint client = HkGetClientIdFromCharname(cmd->GetAdminName());
		PlayerBase *base = GetPlayerBaseForClient(client);

		uint ship;
		pub::Player::GetShip(client, ship);
		if (!ship)
		{
			PrintUserCmdText(client, L"ERR Not in space");
			return true;
		}

		// If the ship is moving, abort the processing.
		Vector dir1;
		Vector dir2;
		pub::SpaceObj::GetMotion(ship, dir1, dir2);
		if (dir1.x > 5 || dir1.y > 5 || dir1.z > 5)
		{
			PrintUserCmdText(client, L"ERR Ship is moving");
			return true;
		}

		wstring archtype = cmd->ArgStr(1);
		if (!archtype.length())
		{
			PrintUserCmdText(client, L"ERR No archtype");
			PrintUserCmdText(client, L"Usage: .jumpcreate <archtype> <loadout> <type> <dest system> <x> <y> <z> <affiliation> <name>");
			return true;
		}
		wstring loadout = cmd->ArgStr(2);
		if (!loadout.length())
		{
			PrintUserCmdText(client, L"ERR No loadout");
			PrintUserCmdText(client, L"Usage: .jumpcreate <archtype> <loadout> <type> <dest system> <x> <y> <z> <affiliation> <name>");
			return true;
		}
		wstring type = cmd->ArgStr(3);
		if (!type.length())
		{
			PrintUserCmdText(client, L"ERR No type");
			PrintUserCmdText(client, L"Usage: .jumpcreate <archtype> <loadout> <type> <dest system> <x> <y> <z> <affiliation> <name>");
			return true;
		}
		wstring destsystem = cmd->ArgStr(4);
		if (!destsystem.length())
		{
			PrintUserCmdText(client, L"ERR No destination system");
			PrintUserCmdText(client, L"Usage: .jumpcreate <archtype> <loadout> <type> <dest system> <x> <y> <z> <affiliation> <name>");
			return true;
		}

		Vector destpos;
		destpos.x = cmd->ArgFloat(5);
		destpos.y = cmd->ArgFloat(6);
		destpos.z = cmd->ArgFloat(7);

		wstring theaffiliation = cmd->ArgStr(8);
		if (!theaffiliation.length())
		{
			PrintUserCmdText(client, L"ERR No affiliation");
			PrintUserCmdText(client, L"Usage: .jumpcreate <archtype> <loadout> <type> <dest system> <x> <y> <z> <affiliation> <name>");
			return true;
		}


		wstring basename = cmd->ArgStrToEnd(9);
		if (!basename.length())
		{
			PrintUserCmdText(client, L"ERR No name entered");
			PrintUserCmdText(client, L"Usage: .jumpcreate <archtype> <loadout> <type> <dest system> <x> <y> <z> <affiliation> <name>");
			return true;
		}



		// Check for conflicting base name
		if (GetPlayerBase(CreateID(PlayerBase::CreateBaseNickname(wstos(basename)).c_str())))
		{
			PrintUserCmdText(client, L"ERR Base name already exists");
			return true;
		}

		wstring charname = (const wchar_t*)Players.GetActiveCharacterName(client);
		AddLog("NOTICE: Base created %s by %s (%s)",
			wstos(basename).c_str(),
			wstos(charname).c_str(),
			wstos(HkGetAccountID(HkGetAccountByCharname(charname))).c_str());

		wstring password = L"nopassword";

		PlayerBase *newbase = new PlayerBase(client, password, basename);
		player_bases[newbase->base] = newbase;
		newbase->affiliation = CreateID(wstos(theaffiliation).c_str());
		newbase->basetype = wstos(type);
		newbase->basesolar = wstos(archtype);
		newbase->baseloadout = wstos(loadout);
		newbase->defense_mode = 4;
		newbase->base_health = 10000000000;

		newbase->destsystem = CreateID(wstos(destsystem).c_str());
		newbase->destposition = destpos;

		for (map<string, ARCHTYPE_STRUCT>::iterator iter = mapArchs.begin(); iter != mapArchs.end(); iter++)
		{

			ARCHTYPE_STRUCT &thearch = iter->second;
			if (iter->first == newbase->basetype)
			{
				newbase->invulnerable = thearch.invulnerable;
				newbase->logic = thearch.logic;
				newbase->radius = thearch.radius;
			}
		}

		newbase->Spawn();
		newbase->Save();

		PrintUserCmdText(client, L"OK: Solar deployed");
		//PrintUserCmdText(client, L"Default administration password is %s", password.c_str());
		return true;
	}
	else if (args.find(L"basecreate") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		RIGHT_CHECK(RIGHT_BASES)

		uint client = HkGetClientIdFromCharname(cmd->GetAdminName());
		PlayerBase *base = GetPlayerBaseForClient(client);

		uint ship;
		pub::Player::GetShip(client, ship);
		if (!ship)
		{
			PrintUserCmdText(client, L"ERR Not in space");
			return true;
		}

		// If the ship is moving, abort the processing.
		Vector dir1;
		Vector dir2;
		pub::SpaceObj::GetMotion(ship, dir1, dir2);
		if (dir1.x > 5 || dir1.y > 5 || dir1.z > 5)
		{
			PrintUserCmdText(client, L"ERR Ship is moving");
			return true;
		}

		wstring password = cmd->ArgStr(1);
		if (!password.length())
		{
			PrintUserCmdText(client, L"ERR No password");
			PrintUserCmdText(client, L"Usage: .basecreate <password> <archtype> <loadout> <type> <name>");
			return true;
		}
		wstring archtype = cmd->ArgStr(2);
		if (!archtype.length())
		{
			PrintUserCmdText(client, L"ERR No archtype");
			PrintUserCmdText(client, L"Usage: .basecreate <password> <archtype> <loadout> <type> <name>");
			return true;
		}
		wstring loadout = cmd->ArgStr(3);
		if (!loadout.length())
		{
			PrintUserCmdText(client, L"ERR No loadout");
			PrintUserCmdText(client, L"Usage: .basecreate <password> <archtype> <loadout> <type> <name>");
			return true;
		}
		wstring type = cmd->ArgStr(4);
		if (!type.length())
		{
			PrintUserCmdText(client, L"ERR No type");
			PrintUserCmdText(client, L"Usage: .basecreate <password> <archtype> <loadout> <type> <name>");
			return true;
		}
		uint theaffiliation = cmd->ArgInt(5);

		wstring basename = cmd->ArgStrToEnd(6);
		if (!basename.length())
		{
			PrintUserCmdText(client, L"ERR No name");
			PrintUserCmdText(client, L"Usage: .basecreate <password> <archtype> <loadout> <type> <name>");
			return true;
		}



		// Check for conflicting base name
		if (GetPlayerBase(CreateID(PlayerBase::CreateBaseNickname(wstos(basename)).c_str())))
		{
			PrintUserCmdText(client, L"ERR Base name already exists");
			return true;
		}

		wstring charname = (const wchar_t*)Players.GetActiveCharacterName(client);
		AddLog("NOTICE: Base created %s by %s (%s)",
			wstos(basename).c_str(),
			wstos(charname).c_str(),
			wstos(HkGetAccountID(HkGetAccountByCharname(charname))).c_str());

		PlayerBase *newbase = new PlayerBase(client, password, basename);
		player_bases[newbase->base] = newbase;
		newbase->affiliation = theaffiliation;
		newbase->basetype = wstos(type);
		newbase->basesolar = wstos(archtype);
		newbase->baseloadout = wstos(loadout);
		newbase->defense_mode = 2;
		newbase->base_health = 10000000000;

		for (map<string, ARCHTYPE_STRUCT>::iterator iter = mapArchs.begin(); iter != mapArchs.end(); iter++)
		{

			ARCHTYPE_STRUCT &thearch = iter->second;
			if (iter->first == newbase->basetype)
			{
				newbase->invulnerable = thearch.invulnerable;
				newbase->logic = thearch.logic;
			}
		}

		newbase->Spawn();
		newbase->Save();

		PrintUserCmdText(client, L"OK: Base deployed");
		PrintUserCmdText(client, L"Default administration password is %s", password.c_str());
		return true;
	}
	else if (args.find(L"basedebugon") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		RIGHT_CHECK(RIGHT_BASES)

		set_plugin_debug = 1;
		cmd->Print(L"OK base debug is on, sure hope you know what you're doing here.\n");
		return true;
	}
	else if (args.find(L"basedebugoff") == 0)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		RIGHT_CHECK(RIGHT_BASES)

		set_plugin_debug = 0;
		cmd->Print(L"OK base debug is off.\n");
		return true;
	}

	return false;
}


void Plugin_Communication_CallBack(PLUGIN_MESSAGE msg, void* data)
{
	returncode = DEFAULT_RETURNCODE;

	if (msg == CUSTOM_BASE_BEAM)
	{
		CUSTOM_BASE_BEAM_STRUCT* info = reinterpret_cast<CUSTOM_BASE_BEAM_STRUCT*>(data); \
			PlayerBase *base = GetPlayerBase(info->iTargetBaseID);
		if (base)
		{
			returncode = SKIPPLUGINS;
			ForcePlayerBaseDock(info->iClientID, base);
			info->bBeamed = true;
		}
	}
	if (msg == CUSTOM_IS_IT_POB)
	{
		CUSTOM_BASE_IS_IT_POB_STRUCT* info = reinterpret_cast<CUSTOM_BASE_IS_IT_POB_STRUCT*>(data);
		PlayerBase *base = GetPlayerBase(info->iBase);
		returncode = SKIPPLUGINS;
		if (base)
		{
			info->bAnswer = true;
		}
	}
	else if (msg == CUSTOM_BASE_IS_DOCKED)
	{
		CUSTOM_BASE_IS_DOCKED_STRUCT* info = reinterpret_cast<CUSTOM_BASE_IS_DOCKED_STRUCT*>(data);
		PlayerBase *base = GetPlayerBaseForClient(info->iClientID);
		if (base)
		{
			returncode = SKIPPLUGINS;
			info->iDockedBaseID = base->base;
		}
	}
	else if (msg == CUSTOM_BASE_LAST_DOCKED)
	{
		CUSTOM_BASE_LAST_DOCKED_STRUCT* info = reinterpret_cast<CUSTOM_BASE_LAST_DOCKED_STRUCT*>(data);
		PlayerBase *base = GetLastPlayerBaseForClient(info->iClientID);
		if (base)
		{
			returncode = SKIPPLUGINS;
			info->iLastDockedBaseID = base->base;
		}
	}
	else if (msg == CUSTOM_JUMP)
	{
		CUSTOM_JUMP_STRUCT* info = reinterpret_cast<CUSTOM_JUMP_STRUCT*>(data);
		uint client = HkGetClientIDByShip(info->iShipID);
		SyncReputationForClientShip(info->iShipID, client);
	}
	else if (msg == CUSTOM_REVERSE_TRANSACTION)
	{
		if (lastTransactionBase)
		{
			CUSTOM_REVERSE_TRANSACTION_STRUCT* info = reinterpret_cast<CUSTOM_REVERSE_TRANSACTION_STRUCT*>(data);
			if (info->iClientID != lastTransactionClientID) {
				ConPrint(L"base: CUSTOM_REVERSE_TRANSACTION: Something is very wrong! Expected client ID %d but got %d\n", lastTransactionClientID, info->iClientID);
				return;
			}
			PlayerBase *base = GetPlayerBaseForClient(info->iClientID);

			MARKET_ITEM &item = base->market_items[lastTransactionArchID];
			int price = (int)item.price * lastTransactionCount;

			base->RemoveMarketGood(lastTransactionArchID, lastTransactionCount);

			pub::Player::AdjustCash(info->iClientID, -price);
			base->ChangeMoney(price);
			base->Save();
		}
	}
	return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Functions to hook */
EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "Base Plugin by cannon";
	p_PI->sShortName = "base";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ClearClientInfo, PLUGIN_ClearClientInfo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterSelect, PLUGIN_HkIServerImpl_CharacterSelect, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&RequestEvent, PLUGIN_HkIServerImpl_RequestEvent, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LaunchPosHook, PLUGIN_LaunchPosHook, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch, PLUGIN_HkIServerImpl_PlayerLaunch, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch_AFTER, PLUGIN_HkIServerImpl_PlayerLaunch_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterSelect_AFTER, PLUGIN_HkIServerImpl_CharacterSelect_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&JumpInComplete, PLUGIN_HkIServerImpl_JumpInComplete, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseEnter, PLUGIN_HkIServerImpl_BaseEnter, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseExit, PLUGIN_HkIServerImpl_BaseExit, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Dock_Call, PLUGIN_HkCb_Dock_Call, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SystemSwitchOutComplete, PLUGIN_HkIServerImpl_SystemSwitchOutComplete, 0));


	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&GFGoodSell, PLUGIN_HkIServerImpl_GFGoodSell, 15));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ReqRemoveItem, PLUGIN_HkIServerImpl_ReqRemoveItem, 15));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ReqRemoveItem_AFTER, PLUGIN_HkIServerImpl_ReqRemoveItem_AFTER, 15));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&GFGoodBuy, PLUGIN_HkIServerImpl_GFGoodBuy, 15));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&GFGoodBuy_AFTER, PLUGIN_HkIServerImpl_GFGoodBuy_AFTER, 15));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ReqAddItem, PLUGIN_HkIServerImpl_ReqAddItem, 15));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ReqAddItem_AFTER, PLUGIN_HkIServerImpl_ReqAddItem_AFTER, 15));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ReqChangeCash, PLUGIN_HkIServerImpl_ReqChangeCash, 15));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ReqSetCash, PLUGIN_HkIServerImpl_ReqSetCash, 15));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ReqEquipment, PLUGIN_HkIServerImpl_ReqEquipment, 11));

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkTimerCheckKick, PLUGIN_HkTimerCheckKick, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ExecuteCommandString_Callback, PLUGIN_ExecuteCommandString_Callback, 0));

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CShip_destroy, PLUGIN_HkIEngine_CShip_destroy, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseDestroyed, PLUGIN_BaseDestroyed, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkCb_AddDmgEntry, PLUGIN_HkCb_AddDmgEntry, 15));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Plugin_Communication_CallBack, PLUGIN_Plugin_Communication, 11));
	return p_PI;
}

void ResetAllBasesShieldStrength() {
	for (map<uint, PlayerBase*>::iterator i = player_bases.begin(); i != player_bases.end(); ++i) {
		i->second->shield_strength_multiplier = base_shield_strength;
		i->second->damage_taken_since_last_threshold = 0;
	}
}

//return value:
// false = all bases vulnerable, true = invulnerable
bool checkBaseVulnerabilityStatus() {

	if (baseVulnerabilityWindows.empty()) {
		return false;
	}

	time_t tNow = time(0);
	struct tm *t = localtime(&tNow);
	uint currHour = t->tm_hour;
	// iterate over configured vulnerability periods to check if we're in one.
	// - in case of timeStart < timeEnd, eg. 5-10, the base will be vulnerable between 5AM and 10AM.
	// - in case of timeStart > timeEnd, eg. 23-2, the base will be vulnerable after 11PM or before 2AM.
	for (list<BASE_VULNERABILITY_WINDOW>::iterator i = baseVulnerabilityWindows.begin(); i != baseVulnerabilityWindows.end(); ++i){
		if((i->start < i->end 
			&& i->start <= currHour && i->end > currHour)
		|| (i->start > i->end
			&& (i->start <= currHour || i->end > currHour))) {
			// if bases are going vulnerable in this tick, reset their damage resistance to default
			if (isGlobalBaseInvulnerabilityActive) {
				ResetAllBasesShieldStrength();
			}
			return false;
		}
	}
	return true;
}