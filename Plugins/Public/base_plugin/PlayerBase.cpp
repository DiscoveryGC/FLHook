#include "Main.h"

PlayerBase::PlayerBase(uint client, const wstring &password, const wstring &the_basename)
	: basename(the_basename),
	base(0), money(0), base_health(0),
	base_level(1), defense_mode(0), proxy_base(0), affiliation(0), siege_mode(false),
	shield_timeout(0), isShieldOn(false), isFreshlyBuilt(true),
	shield_strength_multiplier(base_shield_strength), damage_taken_since_last_threshold(0)
{
	nickname = CreateBaseNickname(wstos(basename));
	base = CreateID(nickname.c_str());

	// The creating ship is an ally by default.
	BasePassword bp;
	bp.pass = password;
	bp.admin = true;
	passwords.emplace_back(bp);
	ally_tags.emplace_back((const wchar_t*)Players.GetActiveCharacterName(client));

	// Setup the base in the current system and at the location 
	// of the player. Rotate the base so that the docking ports
	// face the ship and move the base to just in front of the ship
	uint ship;
	pub::Player::GetShip(client, ship);
	pub::SpaceObj::GetSystem(ship, system);
	pub::SpaceObj::GetLocation(ship, position, rotation);
	Rotate180(rotation);
	TranslateX(position, rotation, 1000);

	// Create the default module and spawn space obj.
	modules.emplace_back((Module*)new CoreModule(this));

	// Setup derived fields
	SetupDefaults();

}

PlayerBase::PlayerBase(const string &the_path)
	: path(the_path), base(0), money(0),
	base_health(0), base_level(0), defense_mode(0), proxy_base(0), affiliation(0), siege_mode(false),
	shield_timeout(0), isShieldOn(false), isFreshlyBuilt(false),
	shield_strength_multiplier(base_shield_strength), damage_taken_since_last_threshold(0)
{
	// Load and spawn base modules
	Load();

	// Setup derived fields
	SetupDefaults();

}

PlayerBase::~PlayerBase()
{
	for (vector<Module*>::iterator i = modules.begin(); i != modules.end(); ++i)
	{
		if (*i)
		{
			delete* i;
		}
	}
}

void PlayerBase::Spawn()
{
	for (vector<Module*>::iterator i = modules.begin(); i != modules.end(); ++i)
	{
		if (*i)
		{
			(*i)->Spawn();
		}
	}

	SyncReputationForBase();
}

bool IsVulnerabilityWindowActive(BASE_VULNERABILITY_WINDOW window, int timeOfDay)
{
	return ((window.start < window.end
			&& window.start <= timeOfDay && window.end > timeOfDay)
		|| (window.start > window.end
			&& (window.start <= timeOfDay || window.end > timeOfDay)));
}

void PlayerBase::CheckVulnerabilityWindow(uint currTime)
{
	int timeOfDay = (currTime % (3600 * 24)) / 60;
	if (IsVulnerabilityWindowActive(vulnerabilityWindow1, timeOfDay))
	{
		if (!vulnerableWindowStatus)
		{
			//Reset the base defenses to default only on the opening of the first vulnerability window
			siege_mode = true;
			SyncReputationForBase();
			shield_strength_multiplier = base_shield_strength;
			damage_taken_since_last_threshold = 0;
			if (shield_reinforcement_threshold_map.count(base_level))
			{
				base_shield_reinforcement_threshold = shield_reinforcement_threshold_map[base_level];
			}
			else
			{
				base_shield_reinforcement_threshold = FLT_MAX;
			}
		}
		vulnerableWindowStatus = true;
	}
	else if (!single_vulnerability_window && IsVulnerabilityWindowActive(vulnerabilityWindow2, timeOfDay))
	{
		if (!vulnerableWindowStatus)
		{
			vulnerableWindowStatus = true;
			siege_mode = true;
			SyncReputationForBase();
		}
	}
	else if (vulnerableWindowStatus)
	{
		vulnerableWindowStatus = false;
		siege_mode = false;
		SyncReputationForBase();
	}
}

// Dispatch timer to modules and exit immediately if the timer indicates
// that this base has been deleted.
bool PlayerBase::Timer(uint curr_time)
{
	if ((curr_time % set_tick_time) == 0 && logic)
	{
		reservedCatalystMap.clear();
		reservedCatalystMap[set_base_crew_type] = base_level * 200;
	}
	if ((curr_time % 60) == 0 && !invulnerable)
	{
		this->CheckVulnerabilityWindow(curr_time);
	}
	for (Module* pobModule : modules)
	{
		if (pobModule)
		{
			bool is_deleted = pobModule->Timer(curr_time);
			if (is_deleted)
				return true;
		}
	}
	return false;
}

void PlayerBase::SetupDefaults()
{
	// Calculate the hash of the nickname
	if (!proxy_base)
	{
		char system_nick[1024];
		pub::GetSystemNickname(system_nick, sizeof(system_nick), system);

		char proxy_base_nick[1024];
		sprintf(proxy_base_nick, "%s_proxy_base", system_nick);

		proxy_base = CreateID(proxy_base_nick);
	}

	// The path to the save file for the base.
	if (!path.size())
	{
		char datapath[MAX_PATH];
		GetUserDataPath(datapath);

		char tpath[1024];
		sprintf(tpath, R"(%s\Accts\MultiPlayer\player_bases\base_%08x.ini)", datapath, base);
		path = tpath;
	}

	// Build the infocard text
	infocard.clear();
	for (int i = 1; i <= MAX_PARAGRAPHS; i++)
	{
		wstring& wscXML = infocard_para[i];

		if (wscXML.length())
			infocard += L"<TEXT>" + ReplaceStr(wscXML, L"\n", L"</TEXT><PARA/><TEXT>") + L"</TEXT><PARA/><PARA/>";
	}

	// Validate the affiliation and clear it if there is no infocard
	// name assigned to it. We assume that this would be an corrupted affiliation.
	if (affiliation)
	{
		uint name;
		pub::Reputation::GetGroupName(affiliation, name);
		if (!name)
		{
			affiliation = 0;
		}
	}

	if (vulnerabilityWindow1.start == -1 || vulnerabilityWindow2.start == -1)
	{
		vulnerabilityWindow1 = { 10 * 60, ((10 * 60) + vulnerability_window_length) % (60 * 24) };
		vulnerabilityWindow2 = { 20 * 60, ((20 * 60) + vulnerability_window_length) % (60 * 24) };
	}
	CheckVulnerabilityWindow(time(nullptr));

	if (modules.size() < (base_level * 3) + 1)
	{
		modules.resize((base_level * 3) + 1);
	}
}

void PlayerBase::Load()
{
	INI_Reader ini;
	BuildModule* coreConstruction = nullptr;
	uint moduleCounter = 0;
	modules.resize(1);
	if (ini.open(path.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("Base"))
			{
				int paraindex = 0;
				invulnerable = 0;
				logic = 1;
				string defaultsystem = "iw09";

				while (ini.read_value())
				{
					if (ini.is_value("nickname"))
					{
						nickname = ini.get_value_string();
					}
					else if (ini.is_value("basetype"))
					{
						basetype = ini.get_value_string();
					}
					else if (ini.is_value("basesolar"))
					{
						basesolar = ini.get_value_string();
					}
					else if (ini.is_value("baseloadout"))
					{
						baseloadout = ini.get_value_string();
					}
					else if (ini.is_value("upgrade"))
					{
						base_level = ini.get_value_int(0);
						modules.resize((base_level * 3) + 1);
					}
					else if (ini.is_value("affiliation"))
					{
						affiliation = ini.get_value_int(0);
					}
					else if (ini.is_value("system"))
					{
						string sysNickname = ini.get_value_string(0);
						uint systemId = Universe::get_system_id(sysNickname.c_str());
						if (systemId)
						{
							system = systemId;
						}
						else
						{
							system = ini.get_value_int(0);
						}
					}
					else if (ini.is_value("pos"))
					{
						position.x = ini.get_value_float(0);
						position.y = ini.get_value_float(1);
						position.z = ini.get_value_float(2);
					}
					else if (ini.is_value("rot"))
					{
						Vector erot;
						erot.x = ini.get_value_float(0);
						erot.y = ini.get_value_float(1);
						erot.z = ini.get_value_float(2);
						rotation = EulerMatrix(erot);
					}
					else if (ini.is_value("destobject"))
					{
						destObjectName = ini.get_value_string(0);
						destObject = CreateID(destObjectName.c_str());
					}
					else if (ini.is_value("destsystem"))
					{
						string sysNickname = ini.get_value_string(0);
						uint systemId = Universe::get_system_id(sysNickname.c_str());
						if (systemId)
						{
							destSystem = systemId;
						}
						else
						{
							destSystem = ini.get_value_int(0);
						}
					}
					else if (ini.is_value("destpos"))
					{
						destPos = { ini.get_value_float(0), ini.get_value_float(1), ini.get_value_float(2) };
					}
					else if (ini.is_value("destori"))
					{
						Vector ori = { ini.get_value_float(0), ini.get_value_float(1), ini.get_value_float(2) };
						destOri = EulerMatrix(ori);
					}
					else if (ini.is_value("logic"))
					{
						logic = ini.get_value_int(0);
					}
					else if (ini.is_value("invulnerable"))
					{
						invulnerable = ini.get_value_int(0);
					}
					else if (ini.is_value("shieldstrength"))
					{
						shield_strength_multiplier = ini.get_value_float(0);
					}
					else if (ini.is_value("shielddmgtaken"))
					{
						damage_taken_since_last_threshold = ini.get_value_float(0);
					}
					else if (ini.is_value("last_vulnerability_change"))
					{
						lastVulnerabilityWindowChange = ini.get_value_int(0);
					}
					else if (ini.is_value("vulnerability_windows"))
					{
						vulnerabilityWindow1 = { ini.get_value_int(0) * 60, ((ini.get_value_int(0) * 60) + vulnerability_window_length) % (60 * 24) };
						vulnerabilityWindow2 = { ini.get_value_int(1) * 60, ((ini.get_value_int(1) * 60) + vulnerability_window_length) % (60 * 24) };
					}
					else if (ini.is_value("infoname"))
					{
						ini_get_wstring(ini, basename);
					}
					else if (ini.is_value("infocardpara"))
					{
						ini_get_wstring(ini, infocard_para[++paraindex]);
					}
					else if (ini.is_value("infocardpara2"))
					{
						wstring infopara2;
						ini_get_wstring(ini, infopara2);
						infocard_para[paraindex] += infopara2;
					}
					else if (ini.is_value("money"))
					{
						sscanf(ini.get_value_string(), "%I64d", &money);
					}
					else if (ini.is_value("commodity"))
					{
						MARKET_ITEM mi;
						UINT good = ini.get_value_int(0);
						mi.quantity = ini.get_value_int(1);
						mi.price = ini.get_value_float(2);
						mi.min_stock = ini.get_value_int(3);
						mi.max_stock = ini.get_value_int(4);
						mi.is_public = bool(ini.get_value_int(5));
						market_items[good] = mi;
					}
					else if (ini.is_value("health"))
					{
						base_health = ini.get_value_float(0);
					}
					else if (ini.is_value("defensemode"))
					{
						defense_mode = ini.get_value_int(0);

						if (defense_mode == 0)
						{
							defense_mode = 1;
						}
					}
					else if (ini.is_value("ally_tag"))
					{
						wstring tag;
						ini_get_wstring(ini, tag);
						ally_tags.emplace_back(tag);
					}
					else if (ini.is_value("hostile_tag"))
					{
						wstring tag;
						ini_get_wstring(ini, tag);
						// TODO: enable this to load hostile tags hostile_tags[tag] = tag;
						//Useless as perma hostile tags have been implemented
					}
					else if (ini.is_value("perma_hostile_tag"))
					{
						wstring tag;
						ini_get_wstring(ini, tag);
						perma_hostile_tags.emplace_back(tag);
					}
					else if (ini.is_value("faction_ally_tag"))
					{
						ally_factions.insert(ini.get_value_int(0));
					}
					else if (ini.is_value("faction_hostile_tag"))
					{
						hostile_factions.insert(ini.get_value_int(0));
					}
					else if (ini.is_value("passwd"))
					{
						wstring passwd;
						ini_get_wstring(ini, passwd);
						BasePassword bp;
						bp.pass = GetParam(passwd, ' ', 0);
						if (GetParam(passwd, ' ', 1) == L"viewshop")
						{
							bp.viewshop = true;
						}
						else {
							bp.admin = true;
						}
						passwords.emplace_back(bp);
					}
					else if (ini.is_value("blueprint"))
					{
						available_blueprints.insert(CreateID(ini.get_value_string(0)));
					}
					else if (ini.is_value("crew_supplied"))
					{
						isCrewSupplied = ini.get_value_bool(0);
					}
				}
				if (basetype.empty())
				{
					basetype = "legacy";
				}
				if (basesolar.empty())
				{
					basesolar = "legacy";
				}
				if (baseloadout.empty())
				{
					baseloadout = "legacy";
				}
				base = CreateID(nickname.c_str());
			}
			else if (ini.is_header("CoreModule"))
			{
				CoreModule* mod = new CoreModule(this);
				mod->LoadState(ini);
				modules.at(moduleCounter) = mod;
				moduleCounter++;
			}
			else if (ini.is_header("BuildModule"))
			{
				BuildModule* mod = new BuildModule(this);
				mod->LoadState(ini);
				if (mod->active_recipe.shortcut_number == Module::TYPE_CORE)
				{
					coreConstruction = mod;
				}
				else
				{
					modules.at(moduleCounter) = mod;
					moduleCounter++;
				}
			}
			else if (ini.is_header("StorageModule"))
			{
				StorageModule* mod = new StorageModule(this);
				mod->LoadState(ini);
				modules.at(moduleCounter) = mod;
				moduleCounter++;
			}
			else if (ini.is_header("DefenseModule"))
			{
				DefenseModule* mod = new DefenseModule(this);
				mod->LoadState(ini);
				modules.at(moduleCounter) = mod;
				moduleCounter++;
			}
			else if (ini.is_header("FactoryModule"))
			{
				FactoryModule* mod = new FactoryModule(this);
				mod->LoadState(ini);
				modules.at(moduleCounter) = mod;
				moduleCounter++;
			}
		}
		if (coreConstruction)
		{
			modules.emplace_back(coreConstruction);
		}
		ini.close();
	}
}

void PlayerBase::Save()
{
	FILE* file = fopen(path.c_str(), "w");
	if (file)
	{
		fprintf(file, "[Base]\n");
		fprintf(file, "nickname = %s\n", nickname.c_str());
		fprintf(file, "basetype = %s\n", basetype.c_str());
		fprintf(file, "basesolar = %s\n", basesolar.c_str());
		fprintf(file, "baseloadout = %s\n", baseloadout.c_str());
		fprintf(file, "upgrade = %u\n", base_level);
		fprintf(file, "affiliation = %u\n", affiliation);
		fprintf(file, "logic = %u\n", logic);
		fprintf(file, "invulnerable = %u\n", invulnerable);
		fprintf(file, "crew_supplied = %u\n", isCrewSupplied ? 1 : 0);
		fprintf(file, "shieldstrength = %f\n", shield_strength_multiplier);
		fprintf(file, "shielddmgtaken = %f\n", damage_taken_since_last_threshold);
		fprintf(file, "last_vulnerability_change = %u\n", lastVulnerabilityWindowChange);
		fprintf(file, "vulnerability_windows = %u, %u\n", vulnerabilityWindow1.start / 60, vulnerabilityWindow2.start / 60);

		fprintf(file, "money = %I64d\n", money);
		auto sysInfo = Universe::get_system(system);
		fprintf(file, "system = %s\n", sysInfo->nickname);
		fprintf(file, "pos = %0.0f, %0.0f, %0.0f\n", position.x, position.y, position.z);

		Vector vRot = MatrixToEuler(rotation);
		fprintf(file, "rot = %0.0f, %0.0f, %0.0f\n", vRot.x, vRot.y, vRot.z);
		if (mapArchs[basetype].ishubreturn)
		{
			const auto& destSystemInfo = Universe::get_system(destSystem);
			fprintf(file, "destsystem = %s\n", destSystemInfo->nickname);

			fprintf(file, "destpos = %0.0f, %0.0f, %0.0f\n", destPos.x, destPos.y, destPos.z);

			Vector destRot = MatrixToEuler(destOri);
			fprintf(file, "destori = %0.0f, %0.0f, %0.0f\n", destRot.x, destRot.y, destRot.z);
		}
		else if (mapArchs[basetype].isjump && destObject && pub::SpaceObj::ExistsAndAlive(destObject) == 0) //0 means alive, -2 dead
		{
			uint destSystemId;
			pub::SpaceObj::GetSystem(destObject, destSystemId);
			const auto& destSystemInfo = Universe::get_system(destSystemId);
			fprintf(file, "destsystem = %s\n", destSystemInfo->nickname);
			fprintf(file, "destobject = %s\n", destObjectName.c_str());
		}

		ini_write_wstring(file, "infoname", basename);
		for (int i = 1; i <= MAX_PARAGRAPHS; i++)
		{
			ini_write_wstring(file, "infocardpara", infocard_para[i].substr(0, 252));
			if (infocard_para[i].length() >= 252)
				ini_write_wstring(file, "infocardpara2", infocard_para[i].substr(252, 252));
		}
		for (auto i : market_items)
		{
			fprintf(file, "commodity = %u, %u, %f, %u, %u, %u\n",
				i.first, i.second.quantity, i.second.price, i.second.min_stock, i.second.max_stock, int(i.second.is_public));
		}
		for (auto i : available_blueprints)
		{
			auto blueprint = blueprintMap.find(i);
			if (blueprint != blueprintMap.end())
			{
				fprintf(file, "blueprint = %s\n", blueprint->second.c_str());
			}
		}

		fprintf(file, "defensemode = %u\n", defense_mode);
		for(auto& i : ally_tags)
		{
			ini_write_wstring(file, "ally_tag", i);
		}
		for (auto i : ally_factions)
		{
			fprintf(file, "faction_ally_tag = %d\n", i);
		}
		for (auto i : hostile_factions)
		{
			fprintf(file, "faction_hostile_tag = %d\n", i);
		}
		for (auto& i : hostile_tags)
		{
			ini_write_wstring(file, "hostile_tag", const_cast<wstring&>(i.first));
		}
		for(auto& i : perma_hostile_tags)
		{
			ini_write_wstring(file, "perma_hostile_tag", i);
		}
		foreach(passwords, BasePassword, i)
		{
			BasePassword bp = *i;
			wstring l = bp.pass;
			if (!bp.admin && bp.viewshop)
			{
				l += L" viewshop";
			}
			ini_write_wstring(file, "passwd", l);
		}
		fprintf(file, "health = %0.0f\n", base_health);

		for (vector<Module*>::iterator i = modules.begin(); i != modules.end(); ++i)
		{
			if (*i)
			{
				(*i)->SaveState(file);
			}
		}

		fclose(file);
	}

	SendBaseStatus(this);
}


bool PlayerBase::AddMarketGood(uint good, uint quantity)
{
	if (quantity == 0)
	{
		return true;
	}

	float vol, mass;
	pub::GetGoodProperties(good, vol, mass);

	if (GetRemainingCargoSpace() < (quantity * vol)
		|| (market_items.count(good) && market_items[good].max_stock < market_items[good].quantity + quantity))
	{
		return false;
	}

	market_items[good].quantity += quantity;
	SendMarketGoodUpdated(this, good, market_items[good]);
	return true;
}

void PlayerBase::RemoveMarketGood(uint good, uint quantity)
{
	auto iter = market_items.find(good);
	if (iter != market_items.end())
	{
		iter->second.quantity = max(0, iter->second.quantity - quantity);
		SendMarketGoodUpdated(this, good, iter->second);
	}
}

void PlayerBase::ChangeMoney(INT64 the_money)
{
	money += the_money;
	if (money < 0)
	{
		money = 0;
	}
}

uint PlayerBase::GetRemainingCargoSpace()
{
	uint used = 0;
	for (auto i = market_items.begin(); i != market_items.end(); ++i)
	{
		float vol, mass;
		pub::GetGoodProperties(i->first, vol, mass);
		used += (uint)((float)i->second.quantity * vol);
	}

	if (used > GetMaxCargoSpace())
	{
		return 0;
	}
	return GetMaxCargoSpace() - used;
}

uint PlayerBase::GetMaxCargoSpace()
{
	uint base_max_capacity = 30000;
	for (auto i : modules)
	{
		if (i && i->type == Module::TYPE_STORAGE)
		{
			base_max_capacity += STORAGE_MODULE_CAPACITY;
		}
	}
	return base_max_capacity;
}

string PlayerBase::CreateBaseNickname(const string& basename)
{
	return string("pb_") + basename;
}

uint PlayerBase::HasMarketItem(uint good)
{
	auto i = market_items.find(good);
	if (i != market_items.end())
	{
		return i->second.quantity;
	}
	return 0;
}


float PlayerBase::GetAttitudeTowardsClient(uint client, bool emulated_siege_mode)
{
	// By default all bases are hostile to everybody.
	float attitude = -1.0;
	wstring charname = (const wchar_t*)Players.GetActiveCharacterName(client);

	// Make base hostile if player is on the perma hostile list. First check so it overrides everything.
	if (siege_mode || emulated_siege_mode)
		for (auto& i : perma_hostile_tags)
		{
			if (charname.find(i) == 0)
			{
				return -1.0;
			}
		}

	// Make base friendly if player is on the friendly list.
	for (auto& i : ally_tags)
	{
		if (charname.find(i) == 0)
		{
			return 1.0;
		}
	}

	// Make base hostile if player is on the hostile list.
	if (!emulated_siege_mode && hostile_tags.find(charname) != hostile_tags.end())
	{
		return -1.0;
	}

	uint playeraff = GetAffliationFromClient(client);
	// Make base hostile if player is on the hostile faction list.
	if ((siege_mode || emulated_siege_mode) && hostile_factions.find(playeraff) != hostile_factions.end())
	{
		return -1.0;
	}

	// Make base friendly if player is on the friendly faction list.
	if (ally_factions.find(playeraff) != ally_factions.end())
	{
		return 1.0;
	}

	// if defense mode 3, at this point if player doesn't match any criteria, give him fireworks
	if ((siege_mode || emulated_siege_mode) && defense_mode == 3)
	{
		return -1.0;
	}

	// at this point, we've ran all the checks, so we can do the IFF stuff.
	if (defense_mode == 1 || defense_mode == 2)
	{
		// If an affiliation is defined then use the player's attitude.
		if (affiliation)
		{
			int rep;
			pub::Player::GetRep(client, rep);
			pub::Reputation::GetGroupFeelingsTowards(rep, affiliation, attitude);

			// if in siege mode, return true affiliation, otherwise clamp to minimum neutralNoDock rep
			if (siege_mode || emulated_siege_mode)
			{
				return attitude;
			}
			else
			{
				return max(-0.59f, attitude);
			}
		}
	}

	// if a player has no standing at all, be neutral otherwise newbies all get shot
	return 0.0;
}

// For all players in the base's system, resync their reps towards all objects
// of this base.
void PlayerBase::SyncReputationForBase()
{
	struct PlayerData* pd = nullptr;
	while (pd = Players.traverse_active(pd))
	{
		if (pd->iShipID && pd->iSystemID == system)
		{
			int player_rep;
			pub::SpaceObj::GetRep(pd->iShipID, player_rep);
			float attitude = GetAttitudeTowardsClient(pd->iOnlineID);
			for (auto& i : modules)
			{
				if (i)
				{
					i->SetReputation(player_rep, attitude);
				}
			}
		}
	}
}

// For all players in the base's system, resync their reps towards this object.
void PlayerBase::SyncReputationForBaseObject(uint space_obj)
{
	struct PlayerData* pd = 0;
	while (pd = Players.traverse_active(pd))
	{
		if (pd->iShipID && pd->iSystemID == system)
		{
			int player_rep;
			pub::SpaceObj::GetRep(pd->iShipID, player_rep);
			float attitude = GetAttitudeTowardsClient(pd->iOnlineID);

			int obj_rep;
			pub::SpaceObj::GetRep(space_obj, obj_rep);
			pub::Reputation::SetAttitude(obj_rep, player_rep, attitude);
		}
	}
}

void ReportAttack(wstring basename, wstring charname, uint system, wstring alert_phrase = L"is under attack by")
{
	wstring wscMsg = L"Base %b %s %p!";
	wscMsg = ReplaceStr(wscMsg, L"%b", basename);
	wscMsg = ReplaceStr(wscMsg, L"%p", charname);
	wscMsg = ReplaceStr(wscMsg, L"%s", alert_phrase);

	const Universe::ISystem* iSys = Universe::get_system(system);
	wstring sysname = stows(iSys->nickname);

	HkMsgS(sysname.c_str(), wscMsg.c_str());

	// Logging
	wstring wscMsgLog = L": Base %b is under attack by %p!";
	wscMsgLog = ReplaceStr(wscMsgLog, L"%p", charname);
	wscMsgLog = ReplaceStr(wscMsgLog, L"%b", basename);
	string scText = wstos(wscMsgLog);
	BaseLogging("%s", scText.c_str());

	return;
}

// For all players in the base's system, resync their reps towards all objects
// of this base.
void PlayerBase::SiegeModChainReaction(uint client)
{
	for (auto& it : player_bases)
	{
		if (it.second->system != this->system || it.second->siege_mode || 
			HkDistance3D(it.second->position, this->position) >= siege_mode_chain_reaction_trigger_distance)
		{
			continue;
		}
		float attitude = it.second->GetAttitudeTowardsClient(client, true);
		if (attitude < -0.55f)
		{
			it.second->siege_mode = true;

			const wstring& charname = (const wchar_t*)Players.GetActiveCharacterName(client);
			ReportAttack(it.second->basename, charname, it.second->system, L"has detected hostile activity at a nearby base by");

			it.second->SyncReputationForBase();
		}
	}
}

// Return true if 
void PlayerBase::SpaceObjDamaged(uint space_obj, uint attacking_space_obj, float curr_hitpoints, float new_hitpoints)
{
	if (invulnerable)
	{
		return;
	}
	float incoming_damage = curr_hitpoints - new_hitpoints;

	// Make sure that the attacking player is hostile.
	uint client = HkGetClientIDByShip(attacking_space_obj);
	if (!client)
	{
		return;
	}
	const wstring& charname = (const wchar_t*)Players.GetActiveCharacterName(client);
	last_attacker = charname;

	if (hostile_tags_damage.find(charname) == hostile_tags_damage.end())
		hostile_tags_damage[charname] = 0;

	hostile_tags_damage[charname] += incoming_damage;

	// Allies are allowed to shoot at the base without the base becoming hostile. We do the ally search
	// after checking to see if this player is on the hostile list because allies don't normally
	// shoot at bases and so this is more efficient than searching the ally list first.
	if (hostile_tags.find(charname) == hostile_tags.end())
	{
		bool is_ally = false;
		for (list<wstring>::iterator i = ally_tags.begin(); i != ally_tags.end(); ++i)
		{
			if (charname.find(*i) == 0)
			{
				is_ally = true;
				break;
			}
		}

		if (!is_ally && (hostile_tags_damage[charname]) > damage_threshold)
		{
			hostile_tags[charname] = charname;

			const wstring& charname = (const wchar_t*)Players.GetActiveCharacterName(client);
			ReportAttack(this->basename, charname, this->system, L"has activated self-defense against");

			SyncReputationForBase();

			if (siege_mode)
				SiegeModChainReaction(client);
		}
	}

	if (!siege_mode && (hostile_tags_damage[charname]) > siege_mode_damage_trigger_level)
	{
		const wstring& charname = (const wchar_t*)Players.GetActiveCharacterName(client);
		ReportAttack(this->basename, charname, this->system, L"siege mode triggered by");

		siege_mode = true;
		SiegeModChainReaction(client);
	}

	// If the shield is not active but could be set a time 
	// to request that it is activated.
	if (!this->shield_timeout && this->isShieldOn == false
		&& this->vulnerableWindowStatus)
	{
		const wstring& charname = (const wchar_t*)Players.GetActiveCharacterName(client);
		ReportAttack(this->basename, charname, this->system);
		if (set_plugin_debug > 1)
		{
			ConPrint(L"PlayerBase::damaged shield active=%u\n", this->shield_timeout);
		}
	}

	this->shield_timeout = time(nullptr) + 60;
	if (!this->isShieldOn)
	{
		this->isShieldOn = true;
		((CoreModule*)this->modules[0])->EnableShieldFuse(true);
	}
}