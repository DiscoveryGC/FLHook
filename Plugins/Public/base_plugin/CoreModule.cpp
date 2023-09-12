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
#include <hookext_exports.h>

CoreModule::CoreModule(PlayerBase *the_base) : Module(TYPE_CORE), base(the_base), space_obj(0), dont_eat(false), 
dont_rust(false)
{
}

CoreModule::~CoreModule()
{
	if (space_obj)
	{
		pub::SpaceObj::Destroy(space_obj, DestroyType::VANISH);
		spaceobj_modules.erase(space_obj);
		space_obj = 0;
	}
}

void CoreModule::Spawn()
{
	if (!space_obj)
	{
		pub::SpaceObj::SolarInfo si;
		memset(&si, 0, sizeof(si));
		si.iFlag = 4;

		char archname[100];
		if (base->basesolar == "legacy")
		{
			_snprintf(archname, sizeof(archname), "dsy_playerbase_%02u", base->base_level);
		}
		else if (base->basesolar == "modern")
		{
			_snprintf(archname, sizeof(archname), "dsy_playerbase_modern_%02u", base->base_level);
		}
		else
		{
			_snprintf(archname, sizeof(archname), base->basesolar.c_str());
		}
		si.iArchID = CreateID(archname);

		if (base->basesolar == "legacy" || base->basesolar == "modern")
		{
			si.iLoadoutID = CreateID(archname);
		}
		else
		{
			si.iLoadoutID = CreateID(base->baseloadout.c_str());
		}

		si.iHitPointsLeft = 1;
		si.iSystemID = base->system;
		si.mOrientation = base->rotation;
		si.vPos = base->position;
		si.Costume.head = CreateID("pi_pirate2_head");
		si.Costume.body = CreateID("pi_pirate8_body");
		si.Costume.lefthand = 0;
		si.Costume.righthand = 0;
		si.Costume.accessories = 0;
		si.iVoiceID = CreateID("atc_leg_m01");
		strncpy_s(si.cNickName, sizeof(si.cNickName), base->nickname.c_str(), base->nickname.size());

		// Check to see if the hook IDS limit has been reached
		static uint solar_ids = 526000;
		if (++solar_ids > 526999)
		{
			solar_ids = 0;
			return;
		}

		// Send the base name to all players that are online
		base->solar_ids = solar_ids;

		wstring basename = base->basename;
		//if (base->affiliation)
		//{
		//	basename = HkGetWStringFromIDS(Reputation::get_name(base->affiliation)) + L" - " + base->basename;
		//}

		struct PlayerData *pd = 0;
		while (pd = Players.traverse_active(pd))
		{
			HkChangeIDSString(pd->iOnlineID, base->solar_ids, basename);
		}


		// Set the base name
		FmtStr infoname(solar_ids, 0);
		infoname.begin_mad_lib(solar_ids); // scanner name
		infoname.end_mad_lib();

		FmtStr infocard(solar_ids, 0);
		infocard.begin_mad_lib(solar_ids); // infocard
		infocard.end_mad_lib();
		pub::Reputation::Alloc(si.iRep, infoname, infocard);

		CreateSolar::SpawnSolar(space_obj, si);
		spaceobj_modules[space_obj] = this;

		// Set base health to reflect saved value unless this is a new base with
		// a health of zero in which case we set it to 5% of the maximum and let
		// players repair it.
		float current;
		pub::SpaceObj::GetHealth(space_obj, current, base->max_base_health);
		if (base->base_health <= 0)
			base->base_health = base->max_base_health * 0.05f;
		else if (base->base_health > base->max_base_health)
			base->base_health = base->max_base_health;
		pub::SpaceObj::SetRelativeHealth(space_obj, base->base_health / base->max_base_health);

		if (shield_reinforcement_threshold_map.count(base->base_level))
			base->base_shield_reinforcement_threshold = shield_reinforcement_threshold_map[base->base_level];
		else
			base->base_shield_reinforcement_threshold = FLT_MAX;

		base->SyncReputationForBaseObject(space_obj);
		if (set_plugin_debug > 1)
			ConPrint(L"CoreModule::created space_obj=%u health=%f\n", space_obj, base->base_health);

		pub::AI::SetPersonalityParams pers = CreateSolar::MakePersonality();
		pub::AI::SubmitState(space_obj, &pers);

		if (mapArchs[base->basetype].mining)
		{
			HookExt::AddMiningObj(space_obj, mapArchs[base->basetype].miningevent);
			spaceobj_modules[space_obj]->mining = true;
		}
		else
		{
			spaceobj_modules[space_obj]->mining = false;
		}

	}
}

wstring CoreModule::GetInfo(bool xml)
{
	return L"Core";
}

void CoreModule::LoadState(INI_Reader &ini)
{
	while (ini.read_value())
	{
		if (ini.is_value("dont_eat"))
		{
			dont_eat = (ini.get_value_int(0) == 1);
		}
		else if (ini.is_value("dont_rust"))
		{
			dont_rust = (ini.get_value_int(0) == 1);
		}
	}
}

void CoreModule::SaveState(FILE *file)
{
	fprintf(file, "[CoreModule]\n");
	fprintf(file, "dont_eat = %d\n", dont_eat);
	fprintf(file, "dont_rust = %d\n", dont_rust);
}

void CoreModule::RepairDamage(float max_base_health)
{
	// We have to add this because of bug abusers
	// Check for Oxygen and Water
	int checkoxygenwater = 0;
	for (map<uint, uint>::iterator i = set_base_crew_consumption_items.begin();
		i != set_base_crew_consumption_items.end(); ++i)
	{
		// Use water and oxygen.
		uint ow_available = base->HasMarketItem(i->first);
		if (ow_available >= 250)
		{
			//HkMsgU(L"oxywater");
			checkoxygenwater += 1;
		}
	}
	// Check for Food
	int checkfood = 0;
	for (map<uint, uint>::iterator i = set_base_crew_food_items.begin();
		i != set_base_crew_food_items.end(); ++i)
	{
		uint food_available = base->HasMarketItem(i->first);
		if (food_available >= 250)
		{
			//HkMsgU(L"food");
			checkfood += 1;
		}
	}

	// no food & no water & no oxygen = RIOTS
	if ((checkfood != 0) && (checkoxygenwater == 2))
	{
		//HkMsgU(L"base can repair");
		// The bigger the base the more damage can be repaired.
		for (uint repair_cycles = 0; repair_cycles < base->base_level; ++repair_cycles)
		{
			foreach(set_base_repair_items, REPAIR_ITEM, item)
			{
				if (base->base_health >= max_base_health)
					return;

				if (base->HasMarketItem(item->good) >= item->quantity)
				{
					base->RemoveMarketGood(item->good, item->quantity);
					base->base_health += repair_per_repair_cycle;
					base->repairing = true;
				}
			}
		}
	}
}

bool CoreModule::Timer(uint time)
{

	if ((time%set_tick_time) != 0 || set_holiday_mode) {
		return false;
	}

	if (space_obj)
	{
		if ((base->logic == 1) || (base->invulnerable == 0))
		{

			uint number_of_crew = base->HasMarketItem(set_base_crew_type);
			bool isCrewSufficient = number_of_crew >= (base->base_level * 200);
			pub::SpaceObj::GetHealth(space_obj, base->base_health, base->max_base_health);

			if (!dont_rust && ((time%set_damage_tick_time) == 0))
			{
				float no_crew_penalty = isCrewSufficient ? 1.0f : no_crew_damage_multiplier;
				float wear_n_tear_modifier = FindWearNTearModifier(base->base_health / base->max_base_health);
				// Reduce hitpoints to reflect wear and tear. This will eventually
				// destroy the base unless it is able to repair itself.
				float damage_taken = (set_damage_per_tick + (set_damage_per_tick * base->base_level)) * wear_n_tear_modifier * no_crew_penalty;
				base->base_health -= damage_taken;
			}

			// Repair damage if we have sufficient crew on the base.
			base->repairing = false;
			if (isCrewSufficient) {
				RepairDamage(base->max_base_health);
				if (dont_eat) {
					// We won't save base health below, so do it here
					float rhealth = base->base_health / base->max_base_health;
					pub::SpaceObj::SetRelativeHealth(space_obj, rhealth);
				}
			}

			if (base->base_health > base->max_base_health)
				base->base_health = base->max_base_health;
			else if (base->base_health <= 0)
				base->base_health = 0;

			if (!dont_eat)
			{
				// Humans use commodity_oxygen, commodity_water. Consume these for
				// the crew or kill 10 crew off and repeat this every 12 hours.
				if (time % 43200 == 0)
				{
					for (map<uint, uint>::iterator i = set_base_crew_consumption_items.begin();
						i != set_base_crew_consumption_items.end(); ++i)
					{
						// Use water and oxygen.
						if (base->HasMarketItem(i->first) >= number_of_crew)
						{
							base->RemoveMarketGood(i->first, number_of_crew);
						}
						// Insufficient water and oxygen, kill crew.
						else
						{
							base->RemoveMarketGood(set_base_crew_type, (number_of_crew >= 10) ? 10 : number_of_crew);
						}
					}

					// Humans use food but may eat one of a number of types.
					uint crew_to_feed = number_of_crew;
					for (map<uint, uint>::iterator i = set_base_crew_food_items.begin();
						i != set_base_crew_food_items.end(); ++i)
					{
						if (!crew_to_feed)
							break;

						uint food_available = base->HasMarketItem(i->first);
						if (food_available)
						{
							uint food_to_use = (food_available >= crew_to_feed) ? crew_to_feed : food_available;
							base->RemoveMarketGood(i->first, food_to_use);
							crew_to_feed -= food_to_use;
						}
					}

					// Insufficent food so kill crew.
					if (crew_to_feed)
					{
						base->RemoveMarketGood(set_base_crew_type, (crew_to_feed >= 10) ? 10 : crew_to_feed);
					}
				}

				// Save the new base health
				float rhealth = base->base_health / base->max_base_health;
				pub::SpaceObj::SetRelativeHealth(space_obj, rhealth);
				if (set_plugin_debug > 1)
					ConPrint(L"CoreModule::timer space_obj=%u health=%f\n", space_obj, base->base_health);

			}
		}
		//else we do not change health, but we still need to send an update to fix the undockable problem. The base either has no logic or is invulnerable, so processing changes is useless.
		else
		{
			float rhealth = base->base_health / base->max_base_health;
			pub::SpaceObj::SetRelativeHealth(space_obj, rhealth);
			//ConPrint(L"CoreModule::timer space_obj=%u health=%f\n", space_obj, base->base_health);
		}

		// if health is 0 then the object will be destroyed but we won't
		// receive a notification of this so emulate it.
		if (base->base_health < 1)
			return SpaceObjDestroyed(space_obj);
	}

	return false;
}

float CoreModule::SpaceObjDamaged(uint space_obj, uint attacking_space_obj, float curr_hitpoints, float new_hitpoints)
{
	base->SpaceObjDamaged(space_obj, attacking_space_obj, curr_hitpoints, new_hitpoints);
	
	if(base->shield_state == PlayerBase::SHIELD_STATE_OFFLINE)
	{
		// shield offline, return expected damage without modifications
		return new_hitpoints;
	}

	if (base->shield_strength_multiplier >= 1.0f || isGlobalBaseInvulnerabilityActive || base->invulnerable == 1)
	{
		// base invulnerable, keep current health value
		return curr_hitpoints;
	}

	float damageTaken;
	if (!siegeWeaponryMap.empty())
	{
		const auto& siegeDamageIter = siegeWeaponryMap.find(iDmgMunitionID);
		if (siegeDamageIter == siegeWeaponryMap.end())
		{
			//Siege gun(s) defined, but this is not one of them, no damage dealt
			return curr_hitpoints;
		}
		else
		{
			//Even with siege gun damage override, it still takes shield strength into the account
			damageTaken = siegeDamageIter->second * (1.0f - base->shield_strength_multiplier);
		}
	}
	else
	{
		damageTaken = ((curr_hitpoints - new_hitpoints) * (1.0f - base->shield_strength_multiplier));
	}

	base->damage_taken_since_last_threshold += damageTaken;
	if (base->damage_taken_since_last_threshold >= base->base_shield_reinforcement_threshold)
	{
		base->damage_taken_since_last_threshold -= base->base_shield_reinforcement_threshold;
		base->shield_strength_multiplier += shield_reinforcement_increment;
	}
	
	return curr_hitpoints - damageTaken;
}

bool CoreModule::SpaceObjDestroyed(uint space_obj, bool moveFile)
{
	if (this->space_obj == space_obj)
	{
		if (set_plugin_debug > 1)
			ConPrint(L"CoreModule::destroyed space_obj=%u\n", space_obj);
		pub::SpaceObj::LightFuse(space_obj, "player_base_explode_fuse", 0);
		spaceobj_modules.erase(space_obj);
		this->space_obj = 0;

		//chunk of code begins here
		//No need to calculate health in this scenario, go straight to drama
		Siege::SiegeAudioCalc(base->base, base->system, base->position, 0);

		//List all players in the system at the time
		list<string> CharsInSystem;
		struct PlayerData *pd = 0;
		while (pd = Players.traverse_active(pd))
		{
			PrintUserCmdText(pd->iOnlineID, L"Base %s destroyed", base->basename.c_str());
			if (pd->iSystemID == base->system)
			{
				const wstring &charname = (const wchar_t*)Players.GetActiveCharacterName(pd->iOnlineID);
				CharsInSystem.push_back(wstos(charname));
			}
		}

		// Logging
		wstring wscMsg = L": Base %b destroyed";
		wscMsg = ReplaceStr(wscMsg, L"%b", base->basename.c_str());
		string scText = wstos(wscMsg);
		BaseLogging("%s", scText.c_str());

		//Base specific logging
		string msg = "Base destroyed. Players in system: ";
		for each (string player in CharsInSystem)
		{
			msg += (player + "; ");
		}
		Log::LogBaseAction(wstos(base->basename), msg.c_str());

		if (!base->last_attacker.empty())
		{
			ConPrint(L"BASE: Base %s destroyed. Last attacker: %s\n", base->basename.c_str(), base->last_attacker.c_str());
		}
		else
		{
			ConPrint(L"BASE: Base %s destroyed\n", base->basename.c_str());
		}

		// Unspawn, delete base and save file.
		DeleteBase(base, moveFile);

		// Careful not to access this as this object will have been deleted by now.
		return true;
	}
	return false;
}

void CoreModule::SetReputation(int player_rep, float attitude)
{
	if (space_obj)
	{
		int obj_rep;
		pub::SpaceObj::GetRep(this->space_obj, obj_rep);
		if (set_plugin_debug > 1)
			ConPrint(L"CoreModule::SetReputation player_rep=%u obj_rep=%u attitude=%f base=%08x\n",
				player_rep, obj_rep, attitude, base->base);
		pub::Reputation::SetAttitude(obj_rep, player_rep, attitude);
	}
}

float CoreModule::FindWearNTearModifier(float currHpPercentage) {
	for (list<WEAR_N_TEAR_MODIFIER>::iterator i = wear_n_tear_mod_list.begin(); i != wear_n_tear_mod_list.end(); ++i) {
		if (i->fromHP < currHpPercentage && i->toHP >= currHpPercentage) {
			return i->modifier;
		}
	}
	return 1.0;
}