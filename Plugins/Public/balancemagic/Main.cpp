// Balance Magic for Discovery FLHook
// September 2018 by Kazinsal etc.
//
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

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
#include <hookext_exports.h>
#include <math.h>

struct DamageAdjust {
	float fighter;
	float transport;
	float gunboat;
	float cruiser;
	float battlecruiser;
	float battleship;
};

int set_iPluginDebug = 0;
float set_fFighterOverkillAdjustment = 0.15f;
int iLoadedDamageAdjusts = 0;

map<unsigned int, struct DamageAdjust> mapDamageAdjust;

/// A return code to indicate to FLHook if we want the hook processing to continue.
PLUGIN_RETURNCODE returncode;

/// Clear client info when a client connects.
void ClearClientInfo(uint iClientID)
{
}

/// Load the configuration
void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;
	mapDamageAdjust.clear();
	iLoadedDamageAdjusts = 0;

	// The path to the configuration file.
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\balancemagic.cfg";

	// Load generic settings
	set_iPluginDebug = IniGetI(scPluginCfgFile, "General", "debug", 0);

	INI_Reader ini;
	if (ini.open(scPluginCfgFile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("DamageAdjust"))
			{
				while (ini.read_value())
				{
					struct DamageAdjust stEntry = { 0.0f };
					stEntry.fighter = ini.get_value_float(0);
					stEntry.transport = ini.get_value_float(1) ? ini.get_value_float(1) : stEntry.fighter;
					stEntry.gunboat = ini.get_value_float(2) ? ini.get_value_float(2) : stEntry.transport;
					stEntry.cruiser = ini.get_value_float(3) ? ini.get_value_float(3) : stEntry.gunboat;
					stEntry.battlecruiser = ini.get_value_float(4) ? ini.get_value_float(4) : stEntry.cruiser;
					stEntry.battleship = ini.get_value_float(5) ? ini.get_value_float(5) : stEntry.battlecruiser;
					mapDamageAdjust[CreateID(ini.get_name_ptr())] = stEntry;
					if (set_iPluginDebug)
						ConPrint(L"%s (%u) = %f, %f, %f, %f, %f, %f\n", stows(ini.get_name_ptr()).c_str(), CreateID(ini.get_name_ptr()), \
							stEntry.fighter, stEntry.transport, stEntry.gunboat, stEntry.cruiser, stEntry.battlecruiser, stEntry.battleship);
					++iLoadedDamageAdjusts;
				}
			}
		}
		ini.close();
	}

	ConPrint(L"BALANCEMAGIC: Loaded %u damage adjusts.\n", iLoadedDamageAdjusts);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	static bool patched = false;
	srand((uint)time(0));

	// If we're being loaded from the command line while FLHook is running then
	// set_scCfgFile will not be empty so load the settings as FLHook only
	// calls load settings on FLHook startup and .rehash.
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		if (set_scCfgFile.length()>0)
			LoadSettings();	
	}
	return true;
}

bool UserCmd_Process(uint client, const wstring &cmd)
{
	returncode = DEFAULT_RETURNCODE;
	return false;
}
	
void __stdcall CharacterSelect(struct CHARACTER_ID const &charid, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
}

void __stdcall HkCb_AddDmgEntry(DamageList *dmg, unsigned short p1, float damage, enum DamageEntry::SubObjFate fate)
{
	returncode = DEFAULT_RETURNCODE;
	uint iDmgFrom = HkGetClientIDByShip(dmg->get_inflictor_id());
	if (iDmgToSpaceID && iDmgFrom)
	{
		float curr, max;
		float adjustedDamage = 0;

		map<unsigned int, struct DamageAdjust>::iterator iter = mapDamageAdjust.find(iDmgMunitionID);
		if (iter != mapDamageAdjust.end())
		{
			unsigned int uArchID = 0;
			unsigned int uTargetType = 0;

			pub::SpaceObj::GetSolarArchetypeID(iDmgToSpaceID, uArchID);
			pub::SpaceObj::GetType(iDmgToSpaceID, uTargetType);

			if (uTargetType != OBJ_FIGHTER && uTargetType != OBJ_FREIGHTER)
				return;

			Archetype::Ship* targetShiparch = Archetype::GetShip(uArchID);
			uint targetShipClass = targetShiparch->iShipClass;
			bool bShieldsUp = true;

			if (p1 == 1)
				pub::SpaceObj::GetHealth(iDmgToSpaceID, curr, max);
			else if (p1 == 65521)
				pub::SpaceObj::GetShieldHealth(iDmgToSpaceID, curr, max, bShieldsUp);

			/*PrintUserCmdText(iDmgFrom, L"HkCb_AddDmgEntry CORONA iDmgToSpaceID=%u get_inflictor_id=%u curr=%0.2f max=%0.0f damage=%0.2f cause=%u is_player=%u player_id=%u fate=%u\n",
				iDmgToSpaceID, dmg->get_inflictor_id(), curr, max, damage, dmg->get_cause(), dmg->is_inflictor_a_player(), dmg->get_inflictor_owner_player(), fate);*/

			if (targetShipClass < 6)
				adjustedDamage = curr - iter->second.fighter / (bShieldsUp ? 2 : 1);
			else if (targetShipClass < 11)
				adjustedDamage = curr - iter->second.transport / (bShieldsUp ? 2 : 1);
			else if (targetShipClass < 13)
				adjustedDamage = curr - iter->second.gunboat / (bShieldsUp ? 2 : 1);
			else if (targetShipClass < 15)
				adjustedDamage = curr - iter->second.cruiser / (bShieldsUp ? 2 : 1);
			else if (targetShipClass < 16)
				adjustedDamage = curr - iter->second.battlecruiser / (bShieldsUp ? 2 : 1);
			else if (targetShipClass < 19)
				adjustedDamage = curr - iter->second.battleship / (bShieldsUp ? 2 : 1);

			dmg->add_damage_entry(p1, adjustedDamage, fate);

			if (set_iPluginDebug)
				PrintUserCmdText(iDmgFrom, L"You hit a ship's hull (ID 0x%08X) with an 0x%08X for %f adjusted damage (%f -> %f) (p1 = %u, fate = %u).", uArchID, iDmgMunitionID, (curr - adjustedDamage), curr, adjustedDamage, p1, fate);
			
			iDmgTo = 0;
			iDmgToSpaceID = 0;
			iDmgMunitionID = 0;
			
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		}
		else
			return;
	}
}

void __stdcall ShipDestroyed(DamageList *dmg, DWORD *ecx, uint iKill)
{
	returncode = DEFAULT_RETURNCODE;

	/*uint iDmgFrom = HkGetClientIDByShip(dmg->get_inflictor_id());
	if (iDmgToSpaceID && iDmgFrom && iDmgMunitionID == CreateID("torpedo01_mark022_ammo"))
	{
		float curr, max;
		pub::SpaceObj::GetHealth(iDmgToSpaceID, curr, max);

		if (iKill) {
			unsigned int uArchID = 0;
			pub::SpaceObj::GetSolarArchetypeID(iDmgToSpaceID, uArchID);
			Archetype::Ship* targetShiparch = Archetype::GetShip(uArchID);
			uint targetShipClass = targetShiparch->iShipClass;

			if (targetShipClass < 6)
			{
				float adjustedDamage = curr - (5000 * set_fFighterOverkillAdjustment);
				//dmg->add_damage_entry(p1, adjustedDamage, fate);
				if (adjustedDamage > 0)
					returncode = NOFUNCTIONCALL;
			}
		}
	}

	if (dmg->get_hit_pts_left(1) > 0 || dmg->is_destroyed() == false)
		returncode = NOFUNCTIONCALL;*/
	//PrintUserCmdText(HkGetClientIdFromCharname(L"NO-F-Harold.Kane") , L"You destroyed a ship (dmg->get_hit_pts_left = %f).", dmg->get_hit_pts_left(1));
}
	
/** Functions to hook */
EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "Balance Magic plugin by Kazinsal";
	p_PI->sShortName = "balancemagic";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ClearClientInfo, PLUGIN_ClearClientInfo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterSelect, PLUGIN_HkIServerImpl_CharacterSelect, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkCb_AddDmgEntry, PLUGIN_HkCb_AddDmgEntry, 0));
//	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ShipDestroyed, PLUGIN_ShipDestroyed, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	return p_PI;
}