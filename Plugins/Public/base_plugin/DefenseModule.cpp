#include "Main.h"

extern bool set_new_spawn;

unordered_map<uint, AICONFIG> defPlatformAIConfig;

void DefenseModule::LoadSettings(const string& path)
{
	INI_Reader ini;
	if (!ini.open(path.c_str(), false))
	{
		return;
	}
	while (ini.read_header())
	{
		if (ini.is_header("PlatformAI"))
		{
			AICONFIG configAI;
			pub::AI::Personality::GunUseStruct gunUse;
			pub::AI::Personality::MissileUseStruct missileUse;
			pub::AI::Personality::JobStruct job;
			uint attackPrefCounter = 0;
			uint attack_subtarget_counter = 0;

			for (int i = 0; i < 13; i++)
			{
				job.attack_order[i].distance = 0.0f;
				job.attack_order[i].type = 0;
				job.attack_order[i].flag = 0;
			}
			for (int i = 0; i < 8; i++)
			{
				job.attack_subtarget_order[i] = 0;
			}

			uint platformType;

			while (ini.read_value())
			{
				if (ini.is_value("type"))
				{
					platformType = ini.get_value_int(0);
				}

				//GunUse
				else if (ini.is_value("gun_fire_interval_time"))
				{
					gunUse.gun_fire_interval_time = ini.get_value_float(0);
				}
				else if (ini.is_value("gun_fire_interval_variance_percent"))
				{
					gunUse.gun_fire_interval_variance_percent = ini.get_value_float(0);
				}
				else if (ini.is_value("gun_fire_burst_interval_time"))
				{
					gunUse.gun_fire_burst_interval_time = ini.get_value_float(0);
				}
				else if (ini.is_value("gun_fire_burst_interval_variance_percent"))
				{
					gunUse.gun_fire_burst_interval_variance_percent = ini.get_value_float(0);
				}
				else if (ini.is_value("gun_fire_no_burst_interval_time"))
				{
					gunUse.gun_fire_no_burst_interval_time = ini.get_value_float(0);
				}
				else if (ini.is_value("gun_fire_accuracy_cone_angle"))
				{
					gunUse.gun_fire_accuracy_cone_angle = ini.get_value_float(0) * 0.017453292f; //game also does this, convert to radians
				}
				else if (ini.is_value("gun_fire_accuracy_power"))
				{
					gunUse.gun_fire_accuracy_power = ini.get_value_float(0);
				}
				else if (ini.is_value("gun_range_threshold"))
				{
					gunUse.gun_range_threshold = ini.get_value_float(0);
				}
				else if (ini.is_value("gun_target_point_switch_time"))
				{
					gunUse.gun_target_point_switch_time = ini.get_value_float(0);
				}
				else if (ini.is_value("fire_style"))
				{
					gunUse.fire_style = ToLower(ini.get_value_string(0)) == "multiple" ? 0 : 1;
				}
				else if (ini.is_value("auto_turret_interval_time"))
				{
					gunUse.auto_turret_interval_time = ini.get_value_float(0);
				}
				else if (ini.is_value("auto_turret_burst_interval_time"))
				{
					gunUse.auto_turret_burst_interval_time = ini.get_value_float(0);
				}
				else if (ini.is_value("auto_turret_no_burst_interval_time"))
				{
					gunUse.auto_turret_no_burst_interval_time = ini.get_value_float(0);
				}
				else if (ini.is_value("auto_turret_burst_interval_variance_percent"))
				{
					gunUse.auto_turret_burst_interval_variance_percent = ini.get_value_float(0);
				}
				else if (ini.is_value("gun_range_threshold_variance_percent"))
				{
					gunUse.gun_range_threshold_variance_percent = ini.get_value_float(0);
				}
				else if (ini.is_value("gun_fire_accuracy_power_npc"))
				{
					gunUse.gun_fire_accuracy_power_npc = ini.get_value_float(0);
				}

				//MissileUse
				else if (ini.is_value("missile_launch_interval_time"))
				{
					missileUse.missile_launch_interval_time = ini.get_value_float(0);
				}
				else if (ini.is_value("missile_launch_interval_variance_percent"))
				{
					missileUse.missile_launch_interval_variance_percent = ini.get_value_float(0);
				}
				else if (ini.is_value("missile_launch_range"))
				{
					missileUse.missile_launch_range = ini.get_value_float(0);
				}
				else if (ini.is_value("missile_launch_cone_angle"))
				{
					missileUse.missile_launch_cone_angle = ini.get_value_float(0) * 0.017453292f; //game also does this, convert to radians
				}
				else if (ini.is_value("missile_launch_interval_time"))
				{
					missileUse.missile_launch_allow_out_of_range = ini.get_value_bool(0);
				}

				//Job
				else if (ini.is_value("wait_for_leader_target"))
				{
					job.wait_for_leader_target = ini.get_value_bool(0);
				}
				else if (ini.is_value("maximum_leader_target_distance"))
				{
					job.maximum_leader_target_distance = ini.get_value_float(0);
				}
				else if (ini.is_value("flee_when_leader_flees_style"))
				{
					job.flee_when_leader_flees_style = ini.get_value_bool(0);
				}
				else if (ini.is_value("scene_toughness_threshold"))
				{
					string val = ToLower(ini.get_value_string(0));
					if (val == "easiest")
					{
						job.scene_toughness_threshold = 0;
					}
					else if (val == "easy")
					{
						job.scene_toughness_threshold = 1;
					}
					else if (val == "equal")
					{
						job.scene_toughness_threshold = 2;
					}
					else if (val == "hard")
					{
						job.scene_toughness_threshold = 3;
					}
					else if (val == "hardest")
					{
						job.scene_toughness_threshold = 4;
					}
					else
					{
						job.scene_toughness_threshold = 2;
					}
				}
				else if (ini.is_value("flee_scene_threat_style"))
				{
					string val = ToLower(ini.get_value_string(0));
					if (val == "easiest")
					{
						job.flee_scene_threat_style = 0;
					}
					else if (val == "easy")
					{
						job.flee_scene_threat_style = 1;
					}
					else if (val == "equal")
					{
						job.flee_scene_threat_style = 2;
					}
					else if (val == "hard")
					{
						job.flee_scene_threat_style = 3;
					}
					else if (val == "hardest")
					{
						job.flee_scene_threat_style = 4;
					}
					else
					{
						job.flee_scene_threat_style = 2;
					}
				}
				else if (ini.is_value("flee_when_hull_damaged_percent"))
				{
					job.flee_when_hull_damaged_percent = ini.get_value_float(0);
				}
				else if (ini.is_value("flee_no_weapons_style"))
				{
					job.flee_no_weapons_style = ini.get_value_bool(0);
				}
				else if (ini.is_value("loot_flee_threshold"))
				{
					string val = ToLower(ini.get_value_string(0));
					if (val == "easiest")
					{
						job.loot_flee_threshold = 0;
					}
					else if (val == "easy")
					{
						job.loot_flee_threshold = 1;
					}
					else if (val == "equal")
					{
						job.loot_flee_threshold = 2;
					}
					else if (val == "hard")
					{
						job.loot_flee_threshold = 3;
					}
					else if (val == "hardest")
					{
						job.loot_flee_threshold = 4;
					}
					else
					{
						job.loot_flee_threshold = 2;
					}
				}
				else if (ini.is_value("field_targeting"))
				{
					string val = ToLower(ini.get_value_string());
					if (val == "never")
					{
						job.field_targeting = 0;
					}
					else if (val == "low_density")
					{
						job.field_targeting = 1;
					}
					else if (val == "high_density")
					{
						job.field_targeting = 2;
					}
					else if (val == "always")
					{
						job.field_targeting = 3;
					}
					else
					{
						job.field_targeting = 1;
					}

				}
				else if (ini.is_value("loot_preference"))
				{
					job.loot_preference = ini.get_value_int(0);
				}
				else if (ini.is_value("combat_drift_distance"))
				{
					job.combat_drift_distance = ini.get_value_float(0);
				}
				else if (ini.is_value("attack_subtarget_order"))
				{
					string val = ToLower(ini.get_value_string(0));
					int flag;
					if (val == "anything")
					{
						flag = 6;
					}
					else if (val == "guns")
					{
						flag = 0;
					}
					else if (val == "turrets")
					{
						flag = 1;
					}
					else if (val == "launchers")
					{
						flag = 2;
					}
					else if (val == "towers")
					{
						flag = 3;
					}
					else if (val == "engines")
					{
						flag = 4;
					}
					else if (val == "hull")
					{
						flag = 5;
					}

					job.attack_subtarget_order[attack_subtarget_counter] = flag;
					attack_subtarget_counter++;
				}
				else if (ini.is_value("attack_preference"))
				{
					string shipType = ToLower(ini.get_value_string(0));
					uint shipIndex = 0;
					if (shipType == "fighter")
						shipIndex = 0;
					else if (shipType == "freighter")
						shipIndex = 1;
					else if (shipType == "transport")
						shipIndex = 2;
					else if (shipType == "gunboat")
						shipIndex = 3;
					else if (shipType == "cruiser")
						shipIndex = 4;
					else if (shipType == "capital")
						shipIndex = 5;
					else if (shipType == "weapons_platform")
						shipIndex = 8;
					else if (shipType == "solar")
						shipIndex = 10;
					else if (shipType == "anything")
						shipIndex = 11;

					int flag = 0;
					string flagText = ToLower(ini.get_value_string(2));
					if (flagText.find("guns") != string::npos)
					{
						flag += 1;
					}
					if (flagText.find("guided") != string::npos)
					{
						flag += 2;
					}
					if (flagText.find("unguided") != string::npos)
					{
						flag += 4;
					}
					if (flagText.find("torpedo") != string::npos)
					{
						flag += 8;
					}

					pub::AI::Personality::JobStruct::Tattack_order& atkOrder = job.attack_order[attackPrefCounter];
					atkOrder.distance = ini.get_value_float(1);
					atkOrder.flag = flag;
					atkOrder.type = shipIndex;
					attackPrefCounter++;
				}
			}
			job.attack_subtarget_order[attack_subtarget_counter] = 7; // Set the element after the last populated one to the delimiter value: 7
			job.attack_order[attackPrefCounter].type = 12; // Set the element after the last populated one to the delimiter value: 12
			configAI.gunUse = gunUse;
			configAI.missileUse = missileUse;
			configAI.job = job;
			defPlatformAIConfig[platformType] = configAI;
		}
	}
	ini.close();
}

static void InsertConfigIntoPersonality(pub::AI::SetPersonalityParams& pers, uint type)
{
	auto& aiConf = defPlatformAIConfig.find(type);
	if (aiConf == defPlatformAIConfig.end())
	{
		return;
	}

	pers.personality.Job = aiConf->second.job;
	pers.personality.GunUse = aiConf->second.gunUse;
	pers.personality.MissileUse = aiConf->second.missileUse;
}

static uint CreateWPlatformNPC(uint iSystem, Vector position, Matrix rotation, uint solar_ids, uint type)
{
	pub::SpaceObj::ShipInfo si;
	memset(&si, 0, sizeof(si));
	si.iFlag = 4;
	si.iSystem = iSystem;
	si.vPos = position;
	si.mOrientation = rotation;

	switch (type)
	{
	case Module::TYPE_DEFENSE_3:
		si.iShipArchetype = CreateID("wplatform_pbase_01");
		si.iLoadout = CreateID("wplatform_pbase_loadout03");
		break;
	case Module::TYPE_DEFENSE_2:
		si.iShipArchetype = CreateID("wplatform_pbase_01");
		si.iLoadout = CreateID("wplatform_pbase_loadout02");
		break;
	case Module::TYPE_DEFENSE_1:
	default:
		si.iShipArchetype = CreateID("wplatform_pbase_01");
		si.iLoadout = CreateID("wplatform_pbase_loadout01");
		break;
	}

	si.iLook1 = 0; //CreateID("li_newscaster_head_gen_hat");
	si.iLook2 = 0; //CreateID("pl_female1_journeyman_body");
	si.iComm = 0; //CreateID("comm_br_darcy_female");
	si.iPilotVoice = 0; //CreateID("pilot_f_leg_f01a");
	si.iHealth = -1;
	si.iLevel = 19;

	// Define the string used for the scanner name. Because the
	// following entry is empty, the pilot_name is used. This
	// can be overriden to display the ship type instead.
	FmtStr infoname(0, 0);
	infoname.begin_mad_lib(0);
	//infoname.append_string(solar_ids);  // ids that replaces %s0
	//infoname.append_string(261164); // ids that replaces %s1
	infoname.end_mad_lib();

	// Define the string used for the pilot name. The example
	// below shows the use of multiple part names.
	FmtStr infocard(0, 0);
	infocard.begin_mad_lib(16162); //  = ids of "%s0 %s1"
	infocard.append_string(solar_ids);  // ids that replaces %s0
	infocard.append_string(261164); // ids that replaces %s1
	infocard.end_mad_lib();

	pub::Reputation::Alloc(si.iRep, infoname, infocard);

	uint obj_rep_group;
	pub::Reputation::GetReputationGroup(obj_rep_group, "fc_neutral");
	pub::Reputation::SetAffiliation(si.iRep, obj_rep_group);

	uint space_obj;
	pub::SpaceObj::Create(space_obj, si);

	pub::AI::SetPersonalityParams pers = CreateSolar::MakePersonality();
	InsertConfigIntoPersonality(pers, type);
	pub::AI::SubmitState(space_obj, &pers);

	return space_obj;
}


static void SpawnSolar(unsigned int& spaceID, pub::SpaceObj::SolarInfo const& solarInfo)
{
	// hack server.dll so it does not call create solar packet send
	char* serverHackAddress = (char*)hModServer + 0x2A62A;
	char serverHack[] = { '\xEB' };
	WriteProcMem(serverHackAddress, &serverHack, 1);

	pub::SpaceObj::CreateSolar(spaceID, solarInfo);

	uint dunno;
	IObjInspectImpl* inspect;
	if (GetShipInspect(spaceID, inspect, dunno))
	{
		CSolar* solar = (CSolar*)inspect->cobject();

		// for every player in the same system, send solar creation packet
		struct SOLAR_STRUCT
		{
			byte dunno[0x100];
		};

		SOLAR_STRUCT packetSolar;

		char* address1 = (char*)hModServer + 0x163F0;
		char* address2 = (char*)hModServer + 0x27950;

		// fill struct
		__asm
		{
			pushad
			lea ecx, packetSolar
			mov eax, address1
			call eax
			push solar
			lea ecx, packetSolar
			push ecx
			mov eax, address2
			call eax
			add esp, 8
			popad
		}

		struct PlayerData* pPD = 0;
		while (pPD = Players.traverse_active(pPD))
		{
			if (pPD->iSystemID == solarInfo.iSystemID)
				GetClientInterface()->Send_FLPACKET_SERVER_CREATESOLAR(pPD->iOnlineID, (FLPACKET_CREATESOLAR&)packetSolar);
		}
	}

	// undo the server.dll hack
	char serverUnHack[] = { '\x74' };
	WriteProcMem(serverHackAddress, &serverUnHack, 1);
}


static uint CreateWPlatformSolar(PlayerBase* base, uint iSystem, Vector position, Matrix rotation, uint solar_ids, uint type)
{
	pub::SpaceObj::SolarInfo si;
	memset(&si, 0, sizeof(si));
	si.iFlag = 4;
	si.iSystemID = iSystem;
	si.vPos = position;
	si.mOrientation = rotation;

	switch (type)
	{
	case Module::TYPE_DEFENSE_3:
		si.iArchID = CreateID("wplatform_pbase_01");
		si.iLoadoutID = CreateID("wplatform_pbase_loadout03");
		break;
	case Module::TYPE_DEFENSE_2:
		si.iArchID = CreateID("wplatform_pbase_01");
		si.iLoadoutID = CreateID("wplatform_pbase_loadout02");
		break;
	case Module::TYPE_DEFENSE_1:
	default:
		si.iArchID = CreateID("wplatform_pbase_01");
		si.iLoadoutID = CreateID("wplatform_pbase_loadout01");
		break;
	}

	si.Costume.head = CreateID("pi_pirate2_head");
	si.Costume.body = CreateID("pi_pirate8_body");
	si.Costume.lefthand = 0;
	si.Costume.righthand = 0;
	si.Costume.accessories = 0;
	si.iVoiceID = CreateID("atc_leg_m01");

	string wplatform_nickname = base->nickname + itos(rand());

	strncpy_s(si.cNickName, sizeof(si.cNickName), wplatform_nickname.c_str(), wplatform_nickname.size());

	si.iHitPointsLeft = -1;

	// Set the base name
	FmtStr infoname(solar_ids, 0);
	infoname.begin_mad_lib(solar_ids); // scanner name
	infoname.end_mad_lib();

	FmtStr infocard(solar_ids, 0);
	infocard.begin_mad_lib(solar_ids); // infocard
	infocard.end_mad_lib();
	pub::Reputation::Alloc(si.iRep, infoname, infocard);

	//infocard.begin_mad_lib(16162); //  = ids of "%s0 %s1"
	//infocard.append_string(solar_ids);  // ids that replaces %s0
	//infocard.append_string(261164); // ids that replaces %s1

	uint space_obj;
	SpawnSolar(space_obj, si);

	pub::AI::SetPersonalityParams pers = CreateSolar::MakePersonality();
	InsertConfigIntoPersonality(pers, type);
	pub::AI::SubmitState(space_obj, &pers);

	return space_obj;
}

DefenseModule::DefenseModule(PlayerBase* the_base)
	: Module(Module::TYPE_DEFENSE_1), base(the_base), space_obj(0)
{
	pos = base->position;
	rot = MatrixToEuler(base->rotation);
	TranslateY(pos, base->rotation, 200);
}

DefenseModule::DefenseModule(PlayerBase* the_base, uint the_type)
	: Module(the_type), base(the_base), space_obj(0)
{
	pos = base->position;
	rot = MatrixToEuler(base->rotation);
	TranslateY(pos, base->rotation, 200);
}

DefenseModule::~DefenseModule()
{
	if (space_obj)
	{
		pub::SpaceObj::Destroy(space_obj, DestroyType::VANISH);
		spaceobj_modules.erase(space_obj);
		space_obj = 0;
	}
}

void DefenseModule::Reset()
{
	if (space_obj)
	{
		pub::SpaceObj::Destroy(space_obj, DestroyType::VANISH);
		spaceobj_modules.erase(space_obj);
		space_obj = 0;
	}
}

wstring DefenseModule::GetInfo(bool xml)
{
	switch (type)
	{
	case Module::TYPE_DEFENSE_1:
		return L"Defense Module - Anti-Light Capital";
	case Module::TYPE_DEFENSE_2:
		return L"Defense Module - Anti-Heavy Capital";
	case Module::TYPE_DEFENSE_3:
		return L"Defense Module - Anti-Light Craft";
	default:
		return L"Wibble";
	}
}

// Load module state from ini file.
void DefenseModule::LoadState(INI_Reader& ini)
{
	while (ini.read_value())
	{
		if (ini.is_value("type"))
		{
			type = ini.get_value_int(0);
		}
		else if (ini.is_value("pos"))
		{
			pos.x = ini.get_value_float(0);
			pos.y = ini.get_value_float(1);
			pos.z = ini.get_value_float(2);
		}
		else if (ini.is_value("rot"))
		{
			rot.x = ini.get_value_float(0);
			rot.y = ini.get_value_float(1);
			rot.z = ini.get_value_float(2);
		}
	}
}

// Append module state to the ini file.
void DefenseModule::SaveState(FILE* file)
{
	fprintf(file, "[DefenseModule]\n");
	fprintf(file, "type = %u\n", type);
	fprintf(file, "pos = %0.0f, %0.0f, %0.0f\n", pos.x, pos.y, pos.z);
	fprintf(file, "rot = %0.0f, %0.0f, %0.0f\n", rot.x, rot.y, rot.z);
}

bool DefenseModule::Timer(uint time)
{
	if ((time % set_tick_time) != 0)
		return false;

	if (!space_obj)
	{
		if (set_new_spawn)
			space_obj = CreateWPlatformSolar(base, base->system, pos, EulerMatrix(rot), base->solar_ids, type);
		else
			space_obj = CreateWPlatformNPC(base->system, pos, EulerMatrix(rot), base->solar_ids, type);

		spaceobj_modules[space_obj] = this;
		if (set_plugin_debug > 1)
			ConPrint(L"DefenseModule::created space_obj=%u\n", space_obj);
		base->SyncReputationForBaseObject(space_obj);
	}

	return false;
}

float DefenseModule::SpaceObjDamaged(uint space_obj, uint attacking_space_obj, float curr_hitpoints, float new_hitpoints)
{
	base->SpaceObjDamaged(space_obj, attacking_space_obj, curr_hitpoints, new_hitpoints);
	return new_hitpoints;
}

bool DefenseModule::SpaceObjDestroyed(uint space_obj)
{
	if (this->space_obj == space_obj)
	{
		if (set_plugin_debug > 1)
			ConPrint(L"DefenseModule::destroyed space_obj=%u\n", space_obj);
		spaceobj_modules.erase(space_obj);
		this->space_obj = 0;
		return true;
	}
	return false;
}

void DefenseModule::SetReputation(int player_rep, float attitude)
{
	if (this->space_obj)
	{
		int obj_rep;
		pub::SpaceObj::GetRep(this->space_obj, obj_rep);
		pub::Reputation::SetAttitude(obj_rep, player_rep, attitude);
	}
}