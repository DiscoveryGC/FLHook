/**
 Mining Plugin for FLHook-Plugin
 by Cannon.

0.1:
 Initial release
0.2:
 Use own asteriod field calculations.
0.3:
 On loot-cheat make player's ship explode and log to flhook cheaters.log
0.4:
 Fixed zone calculation problems.
 Added field by field bonus
0.5:
 Fixed the fix for zone calculation problems
 Added commodity modification for fields
1.0:
 Gave up on my own zone calculations and went back to using the FL ones.
 Changed the bonuses to only work if all equipment items are present.
 Changed the configuration file format to make setup a little quicker.
1.1:
 Fixed player bonus initialisation problem.
 Added playerbonus section error messages and fixed annoying warning
 in flserver-errors.log
1.2:
 Changed mined loot to go directly into cargo hold. Also mining only works
 if the floating loot is hit with a mining gun. Regular guns don't work.
 The system now maintains a historical record of mined ore from fields. Fields
 recharge over time and are depleted as they're mined.
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
#include <unordered_map>

static int set_iPluginDebug = 0;
static string set_scStatsPath;
static uint miningMunition = CreateID("dsy_miningturret_ammo");
static bool set_enableNodeMining = false;
static float set_globalModifier = 1.0f;

extern void PrintZones();

unordered_map<uint, map<uint, float>> idBonusMap;

struct ZONE_BONUS
{
	ZONE_BONUS() : fBonus(0.0f), iReplacementLootID(0), fRechargeRate(0), fCurrReserve(100000), fMaxReserve(50000), fMined(0) {}

	wstring scZone;

	// The loot bonus multiplier.
	float fBonus;

	// The hash of the item to replace the dropped 
	uint iReplacementLootID;

	// The recharge rate of the zone. This is the number of units of ore added
	// to the reserve per minute.
	float fRechargeRate;

	// The current amount of ore in the zone. When this gets low, ore gets harder
	// to mine. When it gets to 0, ore is impossible to mine.
	float fCurrReserve;

	// The maximum limit for the amount of ore in the field
	float fMaxReserve;

	// The amount of ore that has been mined.
	float fMined;
};
unordered_map<uint, ZONE_BONUS> set_mapZoneBonus;

struct CLIENT_DATA
{
	CLIENT_DATA() = default;

	uint equippedID = 0;
	uint lootID = 0;
	uint itemCount = 0;
	uint miningEvents = 0;
	uint miningSampleStart = 0;
	float overminedFraction = 0;

	uint LastTimeMessageAboutBeingFull = 0;
};
unordered_map<uint, CLIENT_DATA> mapClients;



/** A return code to indicate to FLHook if we want the hook processing to continue. */
PLUGIN_RETURNCODE returncode;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////


/// Return the factor to modify a mining loot drop by.
static float GetBonus(uint id, uint lootId)
{
	const auto& bonusForId = idBonusMap.find(id);
	if (bonusForId != idBonusMap.end())
	{
		const auto& bonusForLoot = bonusForId->second.find(lootId);
		if (bonusForLoot != bonusForId->second.end()) {
			return bonusForLoot->second;
		}
	}

	return 1.0f;
}

void CheckClientSetup(uint iClientID)
{
	const auto& equipDesc = Players[iClientID].equipDescList.equip;
	for (auto& equip : equipDesc)
	{
		if (!equip.bMounted)
			continue;
		const Archetype::Tractor* itemPtr = dynamic_cast<Archetype::Tractor*>(Archetype::GetEquipment(equip.iArchID));
		if (itemPtr) {
			mapClients[iClientID].equippedID = equip.iArchID;
			return;
		}
	}
	mapClients[iClientID].equippedID = 0;
}

EXPORT void HkTimerCheckKick()
{
	returncode = DEFAULT_RETURNCODE;

	// Perform 60 second tasks. 
	if ((time(nullptr) % 60) == 0)
	{
		char szDataPath[MAX_PATH];
		GetUserDataPath(szDataPath);
		string scStatsPath = string(szDataPath) + "\\Accts\\MultiPlayer\\mining_stats.txt";
		FILE* file = fopen(scStatsPath.c_str(), "w");
		if (file)
			fprintf(file, "[Zones]\n");

		// Recharge the fields
		for (auto& i = set_mapZoneBonus.begin(); i != set_mapZoneBonus.end(); i++)
		{
			auto& zone = i->second;
			zone.fCurrReserve = min(zone.fCurrReserve + zone.fRechargeRate, zone.fMaxReserve);

			if (file && !zone.scZone.empty() && zone.fMaxReserve > 0 && zone.fMaxReserve != zone.fCurrReserve)
			{
				fprintf(file, "%ls, %0.0f, %0.0f\n", zone.scZone.c_str(), zone.fCurrReserve, zone.fMined);
			}
		}

		if (file)
			fclose(file);
	}
}

/// Clear client info when a client connects.
EXPORT void ClearClientInfo(uint iClientID)
{
	auto& cd = mapClients[iClientID];
	cd.equippedID = 0;
	cd.itemCount = 0;
	cd.lootID = 0;
}

/// Load the configuration
EXPORT void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	// The path to the configuration file.
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\minecontrol.cfg";

	// Load generic settings
	set_iPluginDebug = IniGetI(scPluginCfgFile, "MiningGeneral", "Debug", 0);
	set_scStatsPath = IniGetS(scPluginCfgFile, "MiningGeneral", "StatsPath", "");
	set_enableNodeMining = IniGetB(scPluginCfgFile, "MiningGeneral", "NodeMining", false);
	set_globalModifier = IniGetF(scPluginCfgFile, "MiningGeneral", "GlobalModifier", 1.0f);
	if(set_iPluginDebug)
		ConPrint(L"NOTICE: debug=%d\n", set_iPluginDebug);

	// Load the player bonus list and the field bonus list.
	// To receive the bonus for the particular commodity the player has to have 
	// the affiliation (unless this field is empty), one of the ships and 
	// all of the equipment items.
	// The [PlayerBonus] section has the following format:
	// Commodity, Bonus, Affiliation, List of ships, equipment or cargo separated by commas.
	// The [FieldBonus] section has the following format:
	// Field, Bonus, Replacement Commodity
	set_mapZoneBonus.clear();
	INI_Reader ini;
	if (ini.open(scPluginCfgFile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("PlayerBonus"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("pb"))
					{
						uint licenceId = CreateID(ini.get_value_string(0));
						wstring licenceName = stows(ini.get_value_string(0));
						uint lootId = CreateID(ini.get_value_string(1));
						wstring lootName = stows(ini.get_value_string(1));
						float bonus = ini.get_value_float(2);
						if (set_iPluginDebug)
						{
							ConPrint(L"NOTICE: licence %ls bonus=%2.2f loot=%s(%u)\n",
								licenceName.c_str(), bonus, lootName.c_str(), lootId);
						}

						idBonusMap[licenceId][lootId] = bonus;
					}
				}
			}
			else if (ini.is_header("ZoneBonus"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("zb"))
					{
						uint zoneID = CreateID(ini.get_value_string(0));
						wstring zoneName = stows(ini.get_value_string(0));
						float bonus = ini.get_value_float(1);
						uint replacementLootID = CreateID(ini.get_value_string(2));
						wstring replacementLootName = stows(ini.get_value_string(2));
						float rechargeRate = ini.get_value_float(3);
						float maxReserve = ini.get_value_float(4);
						set_mapZoneBonus[zoneID].scZone = zoneName;
						set_mapZoneBonus[zoneID].fBonus = bonus;
						set_mapZoneBonus[zoneID].iReplacementLootID = replacementLootID;
						set_mapZoneBonus[zoneID].fRechargeRate = rechargeRate;
						set_mapZoneBonus[zoneID].fCurrReserve = maxReserve;
						set_mapZoneBonus[zoneID].fMaxReserve = maxReserve;
						if (set_iPluginDebug)
						{
							ConPrint(L"NOTICE: zone bonus %s bonus=%2.2f replacementLootID=%s(%u) rechargeRate=%0.0f maxReserve=%0.0f\n",
								zoneName.c_str(), bonus, replacementLootName.c_str(), replacementLootID, rechargeRate, maxReserve);
						}
					}
				}
			}
		}
		ini.close();
	}

	// Read the last saved zone reserve.
	char szDataPath[MAX_PATH];
	GetUserDataPath(szDataPath);
	string scStatsPath = string(szDataPath) + "\\Accts\\MultiPlayer\\mining_stats.txt";
	if (ini.open(scStatsPath.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("Zones"))
			{
				while (ini.read_value())
				{
					uint zoneID = CreateID(ini.get_value_string(0));
					auto& zoneData = set_mapZoneBonus[zoneID];
					zoneData.fCurrReserve = ini.get_value_float(1);
					zoneData.fMined = ini.get_value_float(2);
				}
			}
		}
		ini.close();
	}

	struct PlayerData *pPD = 0;
	while (pPD = Players.traverse_active(pPD))
	{
		uint iClientID = HkGetClientIdFromPD(pPD);
		ClearClientInfo(iClientID);
	}
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	srand((uint)time(0));
	// If we're being loaded from the command line while FLHook is running then
	// set_scCfgFile will not be empty so load the settings as FLHook only
	// calls load settings on FLHook startup and .rehash.
	if (fdwReason == DLL_PROCESS_ATTACH && set_scCfgFile.length() > 0)
		LoadSettings();
	return true;
}

void __stdcall PlayerLaunch(unsigned int iShip, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	CheckClientSetup(iClientID);
}

/// Called when a gun hits something
void __stdcall SPMunitionCollision(struct SSPMunitionCollisionInfo const & ci, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	// If this is not a lootable rock, do no other processing.
	if (ci.dwTargetShip != 0)
		return;

	returncode = SKIPPLUGINS_NOFUNCTIONCALL;

	if (ci.iProjectileArchID != miningMunition)
		return;

	CLIENT_DATA& cd = mapClients[iClientID];

	if (!cd.itemCount)
		return;

	float itemCount = static_cast<float>(cd.itemCount) + cd.overminedFraction;
	uint lootId = cd.lootID;
	cd.itemCount = 0;
	cd.lootID = 0;

	uint iShip;
	pub::Player::GetShip(iClientID, iShip);

	Vector vPos;
	Matrix mRot;
	pub::SpaceObj::GetLocation(iShip, vPos, mRot);

	uint iClientSystemID;
	pub::Player::GetSystem(iClientID, iClientSystemID);
	CmnAsteroid::CAsteroidSystem* csys = CmnAsteroid::Find(iClientSystemID);
	if (!csys)
		return;

	// Find asteroid field that matches the best.
	for (CmnAsteroid::CAsteroidField* cfield = csys->FindFirst(); cfield; cfield = csys->FindNext())
	{
		if(!cfield->near_field(vPos))
			continue;
		const Universe::IZone *zone = cfield->get_lootable_zone(vPos);
		if (!zone || !zone->lootableZone)
			continue;

		const auto& zoneBonusData = set_mapZoneBonus.find(zone->iZoneID);
		if(zoneBonusData != set_mapZoneBonus.end())
		{
			auto& zoneData = zoneBonusData->second;
			if (zoneData.iReplacementLootID)
				lootId = zoneData.iReplacementLootID;

			if (zoneData.fBonus)
				itemCount *= (zoneData.fBonus + 1.0f);

			if (zoneData.fMaxReserve > 0)
			{
				if (zoneData.fCurrReserve < 1)
					return;
				itemCount = max(itemCount, static_cast<uint>(zoneData.fCurrReserve));
				zoneData.fCurrReserve -= itemCount;
				zoneData.fMined += itemCount;
			}
		}

		itemCount *= GetBonus(cd.equippedID, lootId) * set_globalModifier;
		uint itemCountInt = static_cast<uint>(itemCount);
		cd.overminedFraction = itemCount - itemCountInt;

		// If this ship is has another ship targetted then send the ore into the cargo
		// hold of the other ship.
		uint iSendToClientID = iClientID;
		const Archetype::Equipment* lootInfo = Archetype::GetEquipment(lootId);

		uint iTargetShip;
		pub::SpaceObj::GetTarget(iShip, iTargetShip);
		if (iTargetShip)
		{
			uint iTargetClientID = HkGetClientIDByShip(iTargetShip);
			if (iTargetClientID && HkDistance3DByShip(iShip, iTargetShip) < 1000.0f)
			{
				iSendToClientID = iTargetClientID;
			}
			else if (set_enableNodeMining)
			{
				CLoot* lootObj = dynamic_cast<CLoot*>(CObject::Find(iTargetShip, CObject::CLOOT_OBJECT));
				if (lootObj && lootObj->contents_arch()->iArchID == lootId) {
					uint newUnits = lootObj->get_units() + itemCountInt;
					lootObj->set_units(newUnits);
					if (((uint)time(nullptr) - mapClients[iClientID].LastTimeMessageAboutBeingFull) > 30)
					{
						PrintUserCmdText(iClientID, L"Selected node contains %u units", newUnits);
						mapClients[iClientID].LastTimeMessageAboutBeingFull = (uint)time(nullptr);
					}
					return;
				}
			}
		}

		if (cd.miningSampleStart < time(nullptr))
		{
			float average = cd.miningEvents / 30.0f;
			if (average > 2.0f)
			{
				AddLog("NOTICE: high mining rate charname=%s rate=%0.1f/sec location=%0.0f,%0.0f,%0.0f system=%08x zone=%08x",
					wstos((const wchar_t*)Players.GetActiveCharacterName(iClientID)).c_str(),
					average, vPos.x, vPos.y, vPos.z, zone->iSystemID, zone->iZoneID);
			}

			cd.miningSampleStart = static_cast<uint>(time(nullptr)) + 30;
			cd.miningEvents = 0;
		}

		float fHoldRemaining;
		pub::Player::GetRemainingHoldSize(iSendToClientID, fHoldRemaining);
		if (fHoldRemaining < static_cast<float>(itemCountInt) * lootInfo->fVolume)
		{
			itemCountInt = static_cast<uint>(fHoldRemaining / lootInfo->fVolume);
		}
		if (itemCountInt == 0)
		{
			if (((uint)time(nullptr) - mapClients[iClientID].LastTimeMessageAboutBeingFull) > 1)
			{
				PrintUserCmdText(iClientID, L"%s's cargo is now full.", reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iSendToClientID)));
				pub::Player::SendNNMessage(iClientID, CreateID("insufficient_cargo_space"));
				if (iClientID != iSendToClientID)
				{
					PrintUserCmdText(iSendToClientID, L"Your cargo is now full.");
					pub::Player::SendNNMessage(iSendToClientID, CreateID("insufficient_cargo_space"));
				}
				mapClients[iClientID].LastTimeMessageAboutBeingFull = (uint)time(nullptr);
			}
		}
		else
		{
			pub::Player::AddCargo(iSendToClientID, lootId, itemCountInt, 1.0, false);
		}
		return;
	}
}

void __stdcall MineAsteroid(uint iClientSystemID, class Vector const& vPos, uint iCrateID, uint iLootID, uint iCount, uint iClientID)
{
	//	ConPrint(L"mine_asteroid %d %d %d\n", iCrateID, iLootID, iCount);
	returncode = SKIPPLUGINS_NOFUNCTIONCALL;
	CLIENT_DATA& data = mapClients[iClientID];
	data.itemCount = iCount;
	data.lootID = iLootID;
}

#define IS_CMD(a) !wscCmd.compare(L##a)

bool ExecuteCommandString_Callback(CCmds* cmd, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	if (IS_CMD("printminezones"))
	{
		returncode = NOFUNCTIONCALL;
		PrintZones();
		return true;
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Functions to hook */
EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "Mine Control Plugin by cannon";
	p_PI->sShortName = "minecontrol";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ClearClientInfo, PLUGIN_ClearClientInfo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch, PLUGIN_HkIServerImpl_PlayerLaunch, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&MineAsteroid, PLUGIN_HkIServerImpl_MineAsteroid, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SPMunitionCollision, PLUGIN_HkIServerImpl_SPMunitionCollision, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkTimerCheckKick, PLUGIN_HkTimerCheckKick, 0));
	return p_PI;
}
