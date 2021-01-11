#include "Main.h"

PlayerBase::PlayerBase(uint client, const wstring &password, const wstring &the_basename)
	: basename(the_basename),
	base(0), money(0), base_health(0),
	base_level(1), defense_mode(0), proxy_base(0), affiliation(0),
	repairing(false), shield_active_time(0), shield_state(PlayerBase::SHIELD_STATE_OFFLINE)
{
	nickname = CreateBaseNickname(wstos(basename));
	base = CreateID(nickname.c_str());

	// The creating ship is an ally by default.
	BasePassword bp;
	bp.pass = password;
	bp.admin = true;
	passwords.push_back(bp);
	ally_tags.push_back((const wchar_t*)Players.GetActiveCharacterName(client));

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
	modules.push_back((Module*)new CoreModule(this));

	// Setup derived fields
	SetupDefaults();

	save_timer = rand() % 60;
}

PlayerBase::PlayerBase(const string &the_path)
	: path(the_path), base(0), money(0),
	base_health(0), base_level(0), defense_mode(0), proxy_base(0), affiliation(0),
	repairing(false), shield_active_time(0), shield_state(PlayerBase::SHIELD_STATE_OFFLINE)
{
	// Load and spawn base modules
	Load();

	// Setup derived fields
	SetupDefaults();

	save_timer = rand() % 60;
}

PlayerBase::~PlayerBase()
{
	for (vector<Module*>::iterator i = modules.begin(); i != modules.end(); ++i)
	{
		if (*i)
		{
			delete *i;
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

// Dispatch timer to modules and exit immediately if the timer indicates
// that this base has been deleted.
bool PlayerBase::Timer(uint curr_time)
{
	for (uint i = 0; i < modules.size(); i++)
	{
		Module *module = modules[i];
		if (module)
		{
			bool is_deleted = module->Timer(curr_time);
			if (is_deleted)
				return true;
		}
	}

	// Save base status every 60 seconds.
	if (save_timer-- < 0)
	{
		save_timer = 60;
		Save();
	}

	return false;
}

void PlayerBase::SetupDefaults()
{
	// Resize the to appropriate number of modules.
	modules.resize((base_level * 3) + 1);

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
		sprintf(tpath, "%s\\Accts\\MultiPlayer\\player_bases\\base_%08x.ini", datapath, base);
		path = tpath;
	}

	// Build the infocard text
	infocard.clear();
	for (int i = 1; i <= MAX_PARAGRAPHS; i++)
	{
		wstring wscXML = infocard_para[i];
		if (wscXML.length())
			infocard += L"<TEXT>" + wscXML + L"</TEXT><PARA/><PARA/>";
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
}

void PlayerBase::Load()
{
	INI_Reader ini;
	if (ini.open(path.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("Base"))
			{
				int newsindex = 0;
				int paraindex = 0;
				destposition.x = 0;
				destposition.y = 0;
				destposition.z = 0;
				invulnerable = 0;
				logic = 1;
				string defaultsystem = "iw09";
				destsystem = CreateID(defaultsystem.c_str());

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
					}
					else if (ini.is_value("affiliation"))
					{
						affiliation = ini.get_value_int(0);
					}
					else if (ini.is_value("system"))
					{
						system = ini.get_value_int(0);
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
					else if (ini.is_value("destsystem"))
					{
						destsystem = ini.get_value_int(0);
					}
					else if (ini.is_value("logic"))
					{
						logic = ini.get_value_int(0);
					}
					else if (ini.is_value("invulnerable"))
					{
						invulnerable = ini.get_value_int(0);
					}
					else if (ini.is_value("destposition"))
					{
						destposition.x = ini.get_value_float(0);
						destposition.y = ini.get_value_float(1);
						destposition.z = ini.get_value_float(2);
					}
					else if (ini.is_value("infoname"))
					{
						ini_get_wstring(ini, basename);
					}
					else if (ini.is_value("infocardpara"))
					{
						ini_get_wstring(ini, infocard_para[++paraindex]);
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
						ally_tags.push_back(tag);
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
						perma_hostile_tags.push_back(tag);
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
						passwords.push_back(bp);
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
				CoreModule *mod = new CoreModule(this);
				mod->LoadState(ini);
				modules.push_back(mod);
			}
			else if (ini.is_header("BuildModule"))
			{
				BuildModule *mod = new BuildModule(this);
				mod->LoadState(ini);
				modules.push_back(mod);
			}
			else if (ini.is_header("ShieldModule"))
			{
				ShieldModule *mod = new ShieldModule(this);
				mod->LoadState(ini);
				modules.push_back(mod);
			}
			else if (ini.is_header("StorageModule"))
			{
				StorageModule *mod = new StorageModule(this);
				mod->LoadState(ini);
				modules.push_back(mod);
			}
			else if (ini.is_header("DefenseModule"))
			{
				DefenseModule *mod = new DefenseModule(this);
				mod->LoadState(ini);
				modules.push_back(mod);
			}
			else if (ini.is_header("FactoryModule"))
			{
				FactoryModule *mod = new FactoryModule(this);
				mod->LoadState(ini);
				modules.push_back(mod);
			}
		}
		ini.close();
	}
}

void PlayerBase::Save()
{
	FILE *file = fopen(path.c_str(), "w");
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

		fprintf(file, "money = %I64d\n", money);
		fprintf(file, "system = %u\n", system);
		fprintf(file, "pos = %0.0f, %0.0f, %0.0f\n", position.x, position.y, position.z);

		fprintf(file, "destsystem = %u\n", destsystem);
		fprintf(file, "destposition = %0.0f, %0.0f, %0.0f\n", destposition.x, destposition.y, destposition.z);

		Vector vRot = MatrixToEuler(rotation);
		fprintf(file, "rot = %0.0f, %0.0f, %0.0f\n", vRot.x, vRot.y, vRot.z);

		ini_write_wstring(file, "infoname", basename);
		for (int i = 1; i <= MAX_PARAGRAPHS; i++)
		{
			ini_write_wstring(file, "infocardpara", infocard_para[i]);
		}
		for (map<UINT, MARKET_ITEM>::iterator i = market_items.begin();
			i != market_items.end(); ++i)
		{
			fprintf(file, "commodity = %u, %u, %f, %u, %u\n",
				i->first, i->second.quantity, i->second.price, i->second.min_stock, i->second.max_stock);
		}

		fprintf(file, "defensemode = %u\n", defense_mode);
		foreach(ally_tags, wstring, i)
		{
			ini_write_wstring(file, "ally_tag", *i);
		}
		for (map<wstring, wstring>::iterator i = hostile_tags.begin();
			i != hostile_tags.end(); ++i)
		{
			ini_write_wstring(file, "hostile_tag", (wstring&)i->first);
		}
		foreach(perma_hostile_tags, wstring, i)
		{
			ini_write_wstring(file, "perma_hostile_tag", *i);
		}
		foreach(passwords, BasePassword, i)
		{
			BasePassword bp = *i;
			wstring l = bp.pass;
			if (!bp.admin && bp.viewshop) {
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
	float vol, mass;
	pub::GetGoodProperties(good, vol, mass);

	if (GetRemainingCargoSpace() < (quantity * vol))
		return false;

	market_items[good].quantity += quantity;
	SendMarketGoodUpdated(this, good, market_items[good]);
	return true;
}

void PlayerBase::RemoveMarketGood(uint good, uint quantity)
{
	map<uint, MARKET_ITEM>::iterator iter = market_items.find(good);
	if (iter != market_items.end())
	{
		if (iter->second.quantity <= quantity)
			iter->second.quantity = 0;
		else
			iter->second.quantity -= quantity;
		SendMarketGoodUpdated(this, good, iter->second);
	}
}

void PlayerBase::ChangeMoney(INT64 the_money)
{
	money += the_money;
	if (money < 0)
		money = 0;
}

uint PlayerBase::GetRemainingCargoSpace()
{
	uint used = 0;
	for (map<UINT, MARKET_ITEM>::iterator i = market_items.begin(); i != market_items.end(); ++i)
	{
		float vol, mass;
		pub::GetGoodProperties(i->first, vol, mass);
		used += (uint)((float)i->second.quantity * vol);
	}

	if (used > GetMaxCargoSpace())
		return 0;

	return GetMaxCargoSpace() - used;
}

uint PlayerBase::GetMaxCargoSpace()
{
	uint max_capacity = 30000;
	for (vector<Module*>::iterator i = modules.begin(); i != modules.end(); ++i)
	{
		if ((*i) && (*i)->type == Module::TYPE_STORAGE)
		{
			max_capacity += STORAGE_MODULE_CAPACITY;
		}
	}
	return max_capacity;
}

string PlayerBase::CreateBaseNickname(const string &basename)
{
	return string("pb_") + basename;
}

uint PlayerBase::HasMarketItem(uint good)
{
	map<UINT, MARKET_ITEM>::iterator i = market_items.find(good);
	if (i != market_items.end())
		return i->second.quantity;
	return 0;
}


float PlayerBase::GetAttitudeTowardsClient(uint client)
{
	// By default all bases are hostile to everybody.
	float attitude = -1.0;
	wstring charname = (const wchar_t*)Players.GetActiveCharacterName(client);

	// Make base hostile if player is on the perma hostile list. First check so it overrides everything.
	for (std::list<wstring>::const_iterator i = perma_hostile_tags.begin(); i != perma_hostile_tags.end(); ++i)
	{
		if (charname.find(*i) == 0)
		{
			return -1.0;
		}
	}

	// If defense mode is neutral with restricted or unrestricted set the
	// attitude to neutral.
	if (defense_mode == 0 || defense_mode == 2)
	{
		attitude = 0.0;
	}

	// Make base friendly if player is on the friendly list.
	for (std::list<wstring>::const_iterator i = ally_tags.begin(); i != ally_tags.end(); ++i)
	{
		if (charname.find(*i) == 0)
		{
			return 1.0;
		}
	}

	// Make base hostile if player is on the hostile list.
	if (hostile_tags.find(charname) != hostile_tags.end())
	{
		return -1.0;
	}

	// if defense mode 3, at this point if player doesn't match any criteria, give him fireworks
	if (defense_mode == 3)
	{
		return -1.0;
	}

	// for defense modes 4 and 5, apply zoner neutrality tears
	if (defense_mode == 4 || defense_mode == 5)
	{
		return 0.0;
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
			return attitude;
		}
	}

	// if a player has no standing at all, be neutral otherwise newbies all get shot
	return 0.0;
}

// For all players in the base's system, resync their reps towards all objects
// of this base.
void PlayerBase::SyncReputationForBase()
{
	struct PlayerData *pd = 0;
	while (pd = Players.traverse_active(pd))
	{
		if (pd->iShipID && pd->iSystemID == system)
		{
			int player_rep;
			pub::SpaceObj::GetRep(pd->iShipID, player_rep);
			float attitude = GetAttitudeTowardsClient(pd->iOnlineID);
			for (vector<Module*>::iterator i = modules.begin(); i != modules.end(); ++i)
			{
				if (*i)
				{
					(*i)->SetReputation(player_rep, attitude);
				}
			}
		}
	}
}

// For all players in the base's system, resync their reps towards this object.
void PlayerBase::SyncReputationForBaseObject(uint space_obj)
{
	struct PlayerData *pd = 0;
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

void ReportAttack(wstring basename, wstring charname, uint system)
{
	wstring wscMsg = L"Base %b is under attack by %p!";
	wscMsg = ReplaceStr(wscMsg, L"%b", basename);
	wscMsg = ReplaceStr(wscMsg, L"%p", charname);

	const Universe::ISystem *iSys = Universe::get_system(system);
	wstring sysname = stows(iSys->nickname);

	HkMsgS(sysname.c_str(), wscMsg.c_str());

	// Logging
	wstring wscMsgLog = L": Base %b is under attack by %p!";
	wscMsgLog = ReplaceStr(wscMsgLog, L"%p", charname);
	wscMsgLog = ReplaceStr(wscMsgLog, L"%b", basename);
	string scText = wstos(wscMsgLog);
	BaseLogging("%s", scText.c_str());

	/*

	struct PlayerData *pPD = 0;
	while(pPD = Players.traverse_active(pPD))
	{
	uint iClientsID = HkGetClientIdFromPD(pPD);

	//HkFMsg(iClientsID, wscMsg);

	//wstring msg = L"%base% is under attack!";
	//msg = ReplaceStr(msg, L"%base%", this->basename.c_str());
	}
	*/
	return;
}

// Return true if 
float PlayerBase::SpaceObjDamaged(uint space_obj, uint attacking_space_obj, float curr_hitpoints, float damage)
{
	// Make sure that the attacking player is hostile.
	uint client = HkGetClientIDByShip(attacking_space_obj);
	if (client)
	{
		const wstring &charname = (const wchar_t*)Players.GetActiveCharacterName(client);
		last_attacker = charname;
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

			if (!is_ally)
			{
				if (set_plugin_debug > 1)
					ConPrint(L"PlayerBase::damaged space_obj=%u\n", space_obj);
				
				if (hostile_tags_damage.find(charname) == hostile_tags_damage.end())
					hostile_tags_damage[charname] = 0;

				if ((hostile_tags_damage[charname] + damage) < damage_threshold)
					hostile_tags_damage[charname] += damage;
				else hostile_tags[charname] = charname;
					

				SyncReputationForBase();
			}
		}
	}

	// If the shield is not active but could be set a time 
	// to request that it is activated.
	if (!this->shield_active_time && shield_state == SHIELD_STATE_ONLINE)
	{
		const wstring &charname = (const wchar_t*)Players.GetActiveCharacterName(client);
		ReportAttack(this->basename, charname, this->system);
		this->shield_active_time = 60 + (rand() % 512);
		if (set_plugin_debug > 1)
			ConPrint(L"PlayerBase::damaged shield active=%u\n", this->shield_active_time);
	}

	return 0.0f;
}
