#include "Main.h"

void Utility::SetRepNeutral(uint clientObj, uint targetObj)
{
	int targetRep;
	int droneRep;
	pub::SpaceObj::GetRep(targetObj, targetRep);
	pub::SpaceObj::GetRep(clientObj, droneRep);

	pub::Reputation::SetAttitude(droneRep, targetRep, 0.0f);

}

void Utility::SetRepHostile(uint clientObj, uint targetObj)
{
	int targetRep;
	int droneRep;
	pub::SpaceObj::GetRep(targetObj, targetRep);
	pub::SpaceObj::GetRep(clientObj, droneRep);

	pub::Reputation::SetAttitude(droneRep, targetRep, -1.0f);
}

uint Utility::CreateDroneNameInfocard(const uint& droneOwnerId)
{
	// Allow the plugin to use IDs range 550000 to 550500
	static int currInfocard = 550100;
	if (currInfocard >= 550600)
	{
		// We have bypassed the limit of infocards available. Return the blank infocard '0'
		return 0;
	}

	// Get the playername that we'll be injecting into the drone name infocard
	const wstring &charname = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(droneOwnerId));

	// Create an infocard with the current ID which contains the drone name, and sync it to all players
	const wstring droneName = wstring(L"Drone - ") + charname;

	struct PlayerData *pd = nullptr;
	while((pd = Players.traverse_active(pd)))
	{
		HkChangeIDSString(pd->iOnlineID, currInfocard, droneName);
	}

	// Return the changed infocard and increment the counter
	return currInfocard++;
}

void Utility::LogEvent(const char* szString, ...)
{
	char szBufString[1024];
	va_list marker;
	va_start(marker, szString);
	_vsnprintf(szBufString, sizeof(szBufString) - 1, szString, marker);

	char szBuf[64];
	time_t tNow = time(0);
	struct tm *t = localtime(&tNow);
	strftime(szBuf, sizeof(szBuf), "%d/%m/%Y %H:%M:%S", t);

	FILE *Logfile = fopen(("./flhook_logs/flhook_drones.log"), "at");
	if (Logfile)
	{
		fprintf(Logfile, "%s %s\n", szBuf, szBufString);
		fflush(Logfile);
		fclose(Logfile);
	}
}

void Utility::DeployDrone(uint iClientID, const DroneBuildTimerWrapper& timerWrapper)
{
	// Set the users client state to reflect a drone has been deployed
	clientDroneInfo[iClientID].buildState = STATE_DRONE_LAUNCHED;

	// Get the current system and location of the carrier
	uint iShip;
	Vector shipPos{};
	Matrix shipRot{};
	uint shipSys;

	pub::Player::GetShip(iClientID, iShip);
	pub::SpaceObj::GetLocation(iShip, shipPos, shipRot);
	pub::SpaceObj::GetSystem(iShip, shipSys);

	// If the ship doesn't exist, the carrier has docked. This should already be taken care of by the docking hook, but abort anyways.
	if (!iShip)
	{
		PrintUserCmdText(iClientID, L"Info [DroneBays] :: You've docked and somehow managed to skip a Hook. Contact a dev-team FLHooker about this bug please!");
		return;
	}

	Utility::CreateNPC(iClientID, shipPos, shipRot, shipSys, timerWrapper.reqDrone);
	PrintUserCmdText(iClientID, L"Info :: Drone Launched");
	
	// Log event
	const wstring charname = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID));
	wstring logString = L"Player %s launched drone";
	logString = ReplaceStr(logString, L"%s", charname);
	LogEvent(wstos(logString).c_str());

	// Print launch event to surrounding players
	wstring wscMsg = L"%user has launched a drone";
	wscMsg = ReplaceStr(wscMsg, L"%user", charname);
	PrintLocalUserCmdText(iClientID, wscMsg, 10000);

}

float Utility::RandFloatRange(float a, float b)
{
	return ((b - a)*(static_cast<float>(rand()) / RAND_MAX)) + a;
}

void Utility::CreateNPC(uint iClientID, Vector pos, Matrix rot, uint iSystem, DroneArch drone)
{

	pub::SpaceObj::ShipInfo si;
	memset(&si, 0, sizeof(si));
	si.iFlag = 1;
	si.iSystem = iSystem;
	si.iShipArchetype = drone.archetype;
	si.vPos = pos;
	si.vPos.x = pos.x + RandFloatRange(0, 500);
	si.vPos.y = pos.y + RandFloatRange(0, 500);
	si.vPos.z = pos.z + RandFloatRange(0, 1000);
	si.mOrientation = rot;
	si.iLoadout = drone.loadout;
	si.iLook1 = CreateID("li_newscaster_head_gen_hat");
	si.iLook2 = CreateID("pl_female1_journeyman_body");
	si.iComm = CreateID("comm_br_darcy_female");
	si.iPilotVoice = CreateID("pilot_f_leg_f01a");
	si.iHealth = -1;
	si.iLevel = 23;

	const uint droneNameInfocard = Utility::CreateDroneNameInfocard(iClientID);
	// Define the string used for the scanner name
	FmtStr scanner_name(droneNameInfocard, nullptr);
	scanner_name.begin_mad_lib(droneNameInfocard);
	scanner_name.end_mad_lib();

	// Define the string used for the pilot name. 
	FmtStr pilot_name(droneNameInfocard, nullptr);
	pilot_name.begin_mad_lib(droneNameInfocard);
	pilot_name.end_mad_lib();

	pub::Reputation::Alloc(si.iRep, scanner_name, pilot_name);

	uint iSpaceObj;
	pub::SpaceObj::Create(iSpaceObj, si);

	pub::AI::SetPersonalityParams pers = Utility::MakePersonality();
	pub::AI::SubmitState(iSpaceObj, &pers);

	clientDroneInfo[iClientID].deployedInfo.deployedDroneObj = iSpaceObj;
}

pub::AI::SetPersonalityParams Utility::MakePersonality()
{
	pub::AI::SetPersonalityParams p;
	p.state_graph = pub::StateGraph::get_state_graph("FIGHTER", pub::StateGraph::TYPE_STANDARD);
	p.state_id = true;

	p.personality.EvadeDodgeUse.evade_dodge_style_weight[0] = 0.4f;
	p.personality.EvadeDodgeUse.evade_dodge_style_weight[1] = 0.0f;
	p.personality.EvadeDodgeUse.evade_dodge_style_weight[2] = 0.4f;
	p.personality.EvadeDodgeUse.evade_dodge_style_weight[3] = 0.2f;
	p.personality.EvadeDodgeUse.evade_dodge_cone_angle = 90.0f;
	p.personality.EvadeDodgeUse.evade_dodge_interval_time = 2.0f;
	p.personality.EvadeDodgeUse.evade_dodge_time = 2.0f;
	p.personality.EvadeDodgeUse.evade_dodge_distance = 500.0f;
	p.personality.EvadeDodgeUse.evade_activate_range = 750.0f;
	p.personality.EvadeDodgeUse.evade_dodge_roll_angle = 0.0f;
	p.personality.EvadeDodgeUse.evade_dodge_waggle_axis_cone_angle = 0.0f;
	p.personality.EvadeDodgeUse.evade_dodge_slide_throttle = 0.75f;
	p.personality.EvadeDodgeUse.evade_dodge_turn_throttle = 0.75f;
	p.personality.EvadeDodgeUse.evade_dodge_corkscrew_roll_flip_direction = false;
	p.personality.EvadeDodgeUse.evade_dodge_interval_time_variance_percent = 0.5f;
	p.personality.EvadeDodgeUse.evade_dodge_cone_angle_variance_percent = 0.5f;
	p.personality.EvadeDodgeUse.evade_dodge_direction_weight[0] = 0.25f;
	p.personality.EvadeDodgeUse.evade_dodge_direction_weight[1] = 0.25f;
	p.personality.EvadeDodgeUse.evade_dodge_direction_weight[2] = 0.25f;
	p.personality.EvadeDodgeUse.evade_dodge_direction_weight[3] = 0.25f;

	p.personality.EvadeBreakUse.evade_break_roll_throttle = 0.5f;
	p.personality.EvadeBreakUse.evade_break_time = 5.0f;
	p.personality.EvadeBreakUse.evade_break_interval_time = 1.0f;
	p.personality.EvadeBreakUse.evade_break_afterburner_delay = 0.0f;
	p.personality.EvadeBreakUse.evade_break_turn_throttle = 1.0f;
	p.personality.EvadeBreakUse.evade_break_direction_weight[0] = 1.0f;
	p.personality.EvadeBreakUse.evade_break_direction_weight[1] = 1.0f;
	p.personality.EvadeBreakUse.evade_break_direction_weight[2] = 1.0f;
	p.personality.EvadeBreakUse.evade_break_direction_weight[3] = 1.0f;
	p.personality.EvadeBreakUse.evade_break_style_weight[0] = 1.0f;
	p.personality.EvadeBreakUse.evade_break_style_weight[1] = 1.0f;
	p.personality.EvadeBreakUse.evade_break_style_weight[2] = 1.0f;

	p.personality.BuzzHeadTowardUse.buzz_min_distance_to_head_toward = 900.0f;
	p.personality.BuzzHeadTowardUse.buzz_min_distance_to_head_toward_variance_percent = 0.1f;
	p.personality.BuzzHeadTowardUse.buzz_max_time_to_head_away = 5.0f;
	p.personality.BuzzHeadTowardUse.buzz_head_toward_engine_throttle = 1.0f;
	p.personality.BuzzHeadTowardUse.buzz_head_toward_turn_throttle = 1.0f;
	p.personality.BuzzHeadTowardUse.buzz_head_toward_roll_throttle = 1.0f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_turn_throttle = 1.0f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_cone_angle = 20.0f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_cone_angle_variance_percent = 0.5f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_waggle_axis_cone_angle = 0.0f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_roll_angle = 20.0f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_interval_time = 30.0f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_interval_time_variance_percent = 0.1f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_direction_weight[0] = 0.25f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_direction_weight[1] = 0.25f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_direction_weight[2] = 0.25f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_direction_weight[3] = 0.25f;
	p.personality.BuzzHeadTowardUse.buzz_head_toward_style_weight[0] = 0.33f;
	p.personality.BuzzHeadTowardUse.buzz_head_toward_style_weight[1] = 0.33f;
	p.personality.BuzzHeadTowardUse.buzz_head_toward_style_weight[2] = 0.33f;

	p.personality.BuzzPassByUse.buzz_distance_to_pass_by = 900.0f;
	p.personality.BuzzPassByUse.buzz_pass_by_time = 1.0f;
	p.personality.BuzzPassByUse.buzz_break_direction_cone_angle = 90.0f;
	p.personality.BuzzPassByUse.buzz_break_turn_throttle = 1.0f;
	p.personality.BuzzPassByUse.buzz_drop_bomb_on_pass_by = true;
	p.personality.BuzzPassByUse.buzz_break_direction_weight[0] = 1.0f;
	p.personality.BuzzPassByUse.buzz_break_direction_weight[1] = 1.0f;
	p.personality.BuzzPassByUse.buzz_break_direction_weight[2] = 1.0f;
	p.personality.BuzzPassByUse.buzz_break_direction_weight[3] = 1.0f;
	p.personality.BuzzPassByUse.buzz_pass_by_style_weight[2] = 1.0f;

	p.personality.TrailUse.trail_lock_cone_angle = 30.0f;
	p.personality.TrailUse.trail_break_time = 0.5f;
	p.personality.TrailUse.trail_min_no_lock_time = 0.1f;
	p.personality.TrailUse.trail_break_roll_throttle = 1.0f;
	p.personality.TrailUse.trail_break_afterburner = true;
	p.personality.TrailUse.trail_max_turn_throttle = 1.0f;
	p.personality.TrailUse.trail_distance = 300.0f;

	p.personality.StrafeUse.strafe_run_away_distance = 600.0f;
	p.personality.StrafeUse.strafe_attack_throttle = 1.0f;

	p.personality.EngineKillUse.engine_kill_search_time = 0.0f;
	p.personality.EngineKillUse.engine_kill_face_time = 1.0f;
	p.personality.EngineKillUse.engine_kill_use_afterburner = true;
	p.personality.EngineKillUse.engine_kill_afterburner_time = 2.0f;
	p.personality.EngineKillUse.engine_kill_max_target_distance = 100.0f;

	p.personality.RepairUse.use_shield_repair_pre_delay = 0.0f;
	p.personality.RepairUse.use_shield_repair_post_delay = 1.0f;
	p.personality.RepairUse.use_shield_repair_at_damage_percent = 0.2f;
	p.personality.RepairUse.use_hull_repair_pre_delay = 0.0f;
	p.personality.RepairUse.use_hull_repair_post_delay = 1.0f;
	p.personality.RepairUse.use_hull_repair_at_damage_percent = 0.2f;

	p.personality.GunUse.gun_fire_interval_time = 0.1f;
	p.personality.GunUse.gun_fire_interval_variance_percent = 0.05f;
	p.personality.GunUse.gun_fire_burst_interval_time = 15.0f;
	p.personality.GunUse.gun_fire_burst_interval_variance_percent = 0.05f;
	p.personality.GunUse.gun_fire_no_burst_interval_time = 1.0f;
	p.personality.GunUse.gun_fire_accuracy_cone_angle = 0.00001f;
	p.personality.GunUse.gun_fire_accuracy_power = 100.0f;
	p.personality.GunUse.gun_range_threshold = 1.0f;
	p.personality.GunUse.gun_target_point_switch_time = 0.0f;
	p.personality.GunUse.fire_style = 0;
	p.personality.GunUse.auto_turret_interval_time = 0.1f;
	p.personality.GunUse.auto_turret_burst_interval_time = 15.0f;
	p.personality.GunUse.auto_turret_no_burst_interval_time = 0.1f;
	p.personality.GunUse.auto_turret_burst_interval_variance_percent = 0.1f;
	p.personality.GunUse.gun_range_threshold_variance_percent = 1.0f;
	p.personality.GunUse.gun_fire_accuracy_power_npc = 100.0f;

	p.personality.MineUse.mine_launch_interval = 0.5f;
	p.personality.MineUse.mine_launch_cone_angle = 0.7854f;
	p.personality.MineUse.mine_launch_range = 200.0f;

	p.personality.MissileUse.missile_launch_interval_time = 1.0f;
	p.personality.MissileUse.missile_launch_interval_variance_percent = 0.5f;
	p.personality.MissileUse.missile_launch_range = 1250.0f;
	p.personality.MissileUse.missile_launch_cone_angle = 30.0f;
	p.personality.MissileUse.missile_launch_allow_out_of_range = false;

	p.personality.DamageReaction.evade_break_damage_trigger_percent = 1.0f;
	p.personality.DamageReaction.evade_dodge_more_damage_trigger_percent = 0.25f;
	p.personality.DamageReaction.engine_kill_face_damage_trigger_percent = 1.0f;
	p.personality.DamageReaction.engine_kill_face_damage_trigger_time = 1.0f;
	p.personality.DamageReaction.roll_damage_trigger_percent = 0.3f;
	p.personality.DamageReaction.roll_damage_trigger_time = 1.0f;
	p.personality.DamageReaction.afterburner_damage_trigger_percent = 0.2f;
	p.personality.DamageReaction.afterburner_damage_trigger_time = 0.5f;
	p.personality.DamageReaction.brake_reverse_damage_trigger_percent = 1.0f;
	p.personality.DamageReaction.drop_mines_damage_trigger_percent = 0.25f;
	p.personality.DamageReaction.drop_mines_damage_trigger_time = 0.1f;
	p.personality.DamageReaction.fire_guns_damage_trigger_percent = 1.0f;
	p.personality.DamageReaction.fire_guns_damage_trigger_time = 1.0f;
	p.personality.DamageReaction.fire_missiles_damage_trigger_percent = 1.0f;
	p.personality.DamageReaction.fire_missiles_damage_trigger_time = 1.0f;

	p.personality.MissileReaction.evade_missile_distance = 800.0f;
	p.personality.MissileReaction.evade_break_missile_reaction_time = 1.0f;
	p.personality.MissileReaction.evade_slide_missile_reaction_time = 1.0f;
	p.personality.MissileReaction.evade_afterburn_missile_reaction_time = 1.0f;

	p.personality.CountermeasureUse.countermeasure_active_time = 5.0f;
	p.personality.CountermeasureUse.countermeasure_unactive_time = 0.0f;

	p.personality.FormationUse.force_attack_formation_active_time = 0.0f;
	p.personality.FormationUse.force_attack_formation_unactive_time = 0.0f;
	p.personality.FormationUse.break_formation_damage_trigger_percent = 0.01f;
	p.personality.FormationUse.break_formation_damage_trigger_time = 1.0f;
	p.personality.FormationUse.break_formation_missile_reaction_time = 1.0f;
	p.personality.FormationUse.break_apart_formation_missile_reaction_time = 1.0f;
	p.personality.FormationUse.break_apart_formation_on_evade_break = true;
	p.personality.FormationUse.break_formation_on_evade_break_time = 1.0f;
	p.personality.FormationUse.formation_exit_top_turn_break_away_throttle = 1.0f;
	p.personality.FormationUse.formation_exit_roll_outrun_throttle = 1.0f;
	p.personality.FormationUse.formation_exit_max_time = 5.0f;
	p.personality.FormationUse.formation_exit_mode = 1;

	p.personality.Job.wait_for_leader_target = false;
	p.personality.Job.maximum_leader_target_distance = 3000;
	p.personality.Job.flee_when_leader_flees_style = false;
	p.personality.Job.scene_toughness_threshold = 4;
	p.personality.Job.flee_scene_threat_style = 4;
	p.personality.Job.flee_when_hull_damaged_percent = 0.01f;
	p.personality.Job.flee_no_weapons_style = true;
	p.personality.Job.loot_flee_threshold = 4;
	p.personality.Job.attack_subtarget_order[0] = 5;
	p.personality.Job.attack_subtarget_order[1] = 6;
	p.personality.Job.attack_subtarget_order[2] = 7;
	p.personality.Job.field_targeting = 3;
	p.personality.Job.loot_preference = 0;
	p.personality.Job.combat_drift_distance = 25000;
	p.personality.Job.attack_order[0].distance = 5000;
	p.personality.Job.attack_order[0].type = 11;
	p.personality.Job.attack_order[0].flag = 15;
	p.personality.Job.attack_order[1].type = 12;	

	return p;
}

uint Utility::rand_name()
{
	const int randomIndex = rand() % npcnames.size();
	return npcnames.at(randomIndex);
}
