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
#include <array>

static int set_iPluginDebug = 0;
static string set_scStatsPath;
static float set_miningCheatLogThreshold = 2.0f;
static uint set_miningMunition = CreateID("mining_gun_ammo");
static uint set_deployableContainerCommodity = CreateID("commodity_deployable_container");
static float set_globalModifier = 1.0f;
static float set_containerModifier = 1.05f;
static array<float, 25> set_shipClassModifiers; // using a simple array for optimization purposes
static uint set_containerJettisonCount = 5000;
static uint set_containerLootCrateID = CreateID("lootcrate_ast_loot_metal");
static uint set_containerSolarArchetypeID = CreateID("dsy_playerbase_01");
static uint set_containerLoadoutArchetypeID = CreateID("dsy_playerbase_01");
static bool set_scaleFieldRechargeWithPlayerCount = false;

const uint insufficientCargoSoundId = CreateID("insufficient_cargo_space");

extern void PrintZones();

unordered_map<uint, map<uint, float>> idBonusMap;

struct ZONE_BONUS
{
	ZONE_BONUS() : fMultiplier(0.0f), iReplacementLootID(0), fRechargeRate(0), fCurrReserve(100000), fMaxReserve(50000), fMined(0) {}

	wstring scZone;

	// The loot bonus multiplier.
	float fMultiplier;

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

struct CONTAINER_DATA
{
	uint loot1Id = 0;
	uint loot1Count = 0;
	wstring loot1Name;
	uint lootCrate1Id = 0;
	uint loot2Id = 0;
	uint loot2Count = 0;
	wstring loot2Name;
	uint lootCrate2Id = 0;
	uint nameIDS = 0;
	wstring solarName;
	uint systemId = 0;
	Vector jettisonPos;
	uint clientId = 0;
};

struct CLIENT_DATA
{
	CLIENT_DATA() = default;

	bool initialized = false;
	uint equippedID = 0;
	float equippedVolume = 0.0f;
	uint lootID = 0;
	uint itemCount = 0;
	uint miningEvents = 0;
	uint miningSampleStart = 0;
	float overminedFraction = 0;
	uint deployedContainerId = 0;
	uint lastValidTargetId = 0;
	uint lastValidPlayerId = 0;
	uint lastValidContainerId = 0;
	CONTAINER_DATA* lastValidContainer = nullptr;

	uint LastTimeMessageAboutBeingFull = 0;
};
unordered_map<uint, CLIENT_DATA> mapClients;
unordered_map<uint, CONTAINER_DATA> mapMiningContainers;

/** A return code to indicate to FLHook if we want the hook processing to continue. */
PLUGIN_RETURNCODE returncode;

float GetMiningYieldBonus(const uint id, const uint lootId)
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

void CheckClientSetup(const uint iClientID)
{
	const auto& equipDesc = Players[iClientID].equipDescList.equip;
	bool processedID = false;
	float mountedEquipmentVolume = 0.0f;
	for (auto& equip : equipDesc)
	{
		if (!equip.bMounted && equip.is_internal())
		{
			continue;
		}
		const Archetype::Equipment* itemPtr = Archetype::GetEquipment(equip.iArchID);
		mountedEquipmentVolume += itemPtr->fVolume;
		if (!processedID && itemPtr->get_class_type() == Archetype::TRACTOR)
		{
			processedID = true;
			mapClients[iClientID].equippedID = equip.iArchID;
		}
	}
	mapClients[iClientID].equippedVolume = mountedEquipmentVolume;
}

void DestroyContainer(const uint clientID)
{
	const auto& iter = mapClients.find(clientID);
	if (iter != mapClients.end())
	{
		if (iter->second.deployedContainerId)
		{
			const auto& cd = mapMiningContainers[iter->second.deployedContainerId];
			if (cd.loot1Count)
			{
				Server.MineAsteroid(cd.systemId, cd.jettisonPos, cd.lootCrate1Id, cd.loot1Id, cd.loot1Count, cd.clientId);
			}
			if (cd.loot2Count)
			{
				Server.MineAsteroid(cd.systemId, cd.jettisonPos, cd.lootCrate2Id, cd.loot2Id, cd.loot2Count, cd.clientId);
			}
			Server.MineAsteroid(cd.systemId, cd.jettisonPos, set_containerLootCrateID, set_deployableContainerCommodity, 1, cd.clientId);
			mapMiningContainers.erase(iter->second.deployedContainerId);
			DESPAWN_SOLAR_STRUCT info;
			info.destroyType = FUSE;
			info.spaceObjId = iter->second.deployedContainerId;
			Plugin_Communication(CUSTOM_DESPAWN_SOLAR, &info);
		}
		mapClients[clientID].deployedContainerId = 0;
	}
}

EXPORT void HkTimerCheckKick()
{
	returncode = DEFAULT_RETURNCODE;
	uint currTime = static_cast<uint>(time(nullptr));
	// Perform 120 second tasks. 
	if (currTime % 120 == 0)
	{
		uint playerModifier = 0;
		if (set_scaleFieldRechargeWithPlayerCount)
		{
			PlayerData* pd = nullptr;
			while (pd = Players.traverse_active(pd))
			{
				playerModifier++;
			}
		}
		else
		{
			playerModifier = 1;
		}

		char szDataPath[MAX_PATH];
		GetUserDataPath(szDataPath);
		string scStatsPath = string(szDataPath) + R"(\Accts\MultiPlayer\mining_stats.txt)";
		FILE* file = fopen(scStatsPath.c_str(), "w");
		if (file)
			fprintf(file, "[Zones]\n");

		// Recharge the fields
		for (auto& i = set_mapZoneBonus.begin(); i != set_mapZoneBonus.end(); i++)
		{
			auto& zone = i->second;
			zone.fCurrReserve = min(zone.fCurrReserve + (zone.fRechargeRate * playerModifier), zone.fMaxReserve);

			if (file && !zone.scZone.empty() && zone.fMaxReserve > 0 && zone.fMaxReserve != zone.fCurrReserve)
			{
				fprintf(file, "%ls, %0.0f, %0.0f\n", zone.scZone.c_str(), zone.fCurrReserve, zone.fMined);
			}
		}

		if (file)
		{
			fclose(file);
		}
	}
}

/// Clear client info when a client connects.
EXPORT void ClearClientInfo(uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	auto& cd = mapClients[iClientID];
	cd.equippedID = 0;
	cd.itemCount = 0;
	cd.lootID = 0;
	DestroyContainer(iClientID);
}

/// Load the configuration
EXPORT void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	// The path to the configuration file.
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\minecontrol.cfg";

	set_shipClassModifiers.fill(1.0f);

	// Load generic settings
	set_iPluginDebug = IniGetI(scPluginCfgFile, "MiningGeneral", "Debug", 0);
	set_scStatsPath = IniGetS(scPluginCfgFile, "MiningGeneral", "StatsPath", "");
	set_globalModifier = IniGetF(scPluginCfgFile, "MiningGeneral", "GlobalModifier", set_globalModifier);
	set_containerModifier = IniGetF(scPluginCfgFile, "MiningGeneral", "ContainerModifier", set_containerModifier);
	set_containerJettisonCount = IniGetI(scPluginCfgFile, "MiningGeneral", "ContainerJettisonCount", set_containerJettisonCount);
	set_containerSolarArchetypeID = CreateID(IniGetS(scPluginCfgFile, "MiningGeneral", "ContainerSolarArchetype", "dsy_playerbase_01").c_str());
	set_containerLoadoutArchetypeID = CreateID(IniGetS(scPluginCfgFile, "MiningGeneral", "ContainerLoadoutArchetype", "dsy_playerbase_01").c_str());
	set_deployableContainerCommodity = CreateID(IniGetS(scPluginCfgFile, "MiningGeneral", "ContainerCommodity", "commodity_scrap_metal").c_str());
	set_miningMunition = CreateID(IniGetS(scPluginCfgFile, "MiningGeneral", "MiningMunition", "mining_gun_ammo").c_str());
	set_miningCheatLogThreshold = IniGetF(scPluginCfgFile, "MiningGeneral", "MiningCheatLogThreshold", set_miningCheatLogThreshold);
	set_scaleFieldRechargeWithPlayerCount = IniGetB(scPluginCfgFile, "MiningGeneral", "PlayerScalingRecharge", set_scaleFieldRechargeWithPlayerCount);

	set_containerLootCrateID = Archetype::GetEquipment(set_deployableContainerCommodity)->get_loot_appearance()->iArchID;

	if (set_iPluginDebug)
	{
		ConPrint(L"NOTICE: debug=%d\n", set_iPluginDebug);
	}

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
						if (zoneName.empty() || bonus <= 0.0f || maxReserve <= 0.0f)
						{
							ConPrint(L"Incorrectly setup Zone Bonus entry!\n");
							continue;
						}
						set_mapZoneBonus[zoneID].scZone = zoneName;
						set_mapZoneBonus[zoneID].fMultiplier = bonus;
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
			else if (ini.is_header("ShipTypeBonus"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("ship_class_bonus"))
					{
						set_shipClassModifiers[ini.get_value_int(0)] = ini.get_value_float(1);
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
					string zoneName = ini.get_value_string(0);
					if (zoneName.empty())
					{
						ConPrint(L"Incorrect entry in mining stats file!\n");
						continue;
					}
					uint zoneID = CreateID(zoneName.c_str());
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
	// If we're being loaded from the command line while FLHook is running then
	// set_scCfgFile will not be empty so load the settings as FLHook only
	// calls load settings on FLHook startup and .rehash.
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		if (set_scCfgFile.length() > 0)
		{
			LoadSettings();
		}

		HkLoadStringDLLs();
	}
	if (fdwReason == DLL_PROCESS_DETACH)
	{
		HkUnloadStringDLLs();
	}
	return true;

}


bool UserCmd_Process(uint client, const wstring& args)
{
	returncode = DEFAULT_RETURNCODE;

	if (args.find(L"/cs") != 0 && args.find(L"/cargostored") != 0)
	{
		return false;
	}
	uint targetId;
	uint shipId;
	pub::Player::GetShip(client, shipId);
	pub::SpaceObj::GetTarget(shipId, targetId);
	if (!targetId)
	{
		PrintUserCmdText(client, L"ERR Mining container not selected");
		return true;
	}
	const auto& container = mapMiningContainers.find(targetId);
	if (container == mapMiningContainers.end())
	{
		PrintUserCmdText(client, L"ERR Mining container not selected");
		return true;
	}

	if (!container->second.loot1Count && !container->second.loot2Count)
	{
		PrintUserCmdText(client, L"Container is empty!");
	}
	else if (!container->second.loot2Id)
	{
		PrintUserCmdText(client, L"Container holds %u units of %ls", container->second.loot1Count, container->second.loot1Name.c_str());
	}
	else
	{
		PrintUserCmdText(client, L"Container holds %u units of %ls and %u units of %ls", 
			container->second.loot1Count, container->second.loot1Name.c_str(), container->second.loot2Count, container->second.loot2Name.c_str());
	}

	
	return true;
}

/// Called when a gun hits something
void __stdcall SPMunitionCollision(struct SSPMunitionCollisionInfo const & ci, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	// If this is not a lootable rock, do no other processing.
	if (ci.iProjectileArchID != set_miningMunition || ci.dwTargetShip != 0)
	{
		return;
	}

	returncode = SKIPPLUGINS_NOFUNCTIONCALL;

	CLIENT_DATA& cd = mapClients[iClientID];

	if (!cd.itemCount)
	{
		return;
	}

	// use floats to ensure precision when applying various minor modifiers.
	float miningYield = static_cast<float>(cd.itemCount);
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
	{
		return;
	}

	// Find asteroid field that matches the best.
	for (CmnAsteroid::CAsteroidField* cfield = csys->FindFirst(); cfield; cfield = csys->FindNext())
	{
		if (!cfield->near_field(vPos))
		{
			continue;
		}
		const Universe::IZone* zone = cfield->get_lootable_zone(vPos);
		if (!zone || !zone->lootableZone)
		{
			continue;
		}

		const auto& zoneBonusData = set_mapZoneBonus.find(zone->iZoneID);
		ZONE_BONUS* finalZone = nullptr;
		if(zoneBonusData != set_mapZoneBonus.end())
		{
			auto& zoneData = zoneBonusData->second;
			if (zoneData.fCurrReserve == 0.0f)
			{
				return;
			}

			if (zoneData.iReplacementLootID)
			{
				lootId = zoneData.iReplacementLootID;
			}
			miningYield *= zoneData.fMultiplier;

			miningYield = max(miningYield, zoneData.fCurrReserve);
			finalZone = &zoneData; // save ZONE_BONUS ref to update AFTER all the bonuses are applied
		}
		uint shipClass = Archetype::GetShip(Players[iClientID].iShipArchetype)->iShipClass;

		miningYield *= GetMiningYieldBonus(cd.equippedID, lootId) * set_globalModifier * set_shipClassModifiers[shipClass];
		miningYield += cd.overminedFraction; // add the decimal remainder from last mining event.

		if (finalZone)
		{
			finalZone->fCurrReserve -= miningYield;
			finalZone->fMined += miningYield;
		}
		// If this ship is has another ship targetted then send the ore into the cargo
		// hold of the other ship.
		uint iSendToClientID = iClientID;
		const Archetype::Equipment* lootInfo = Archetype::GetEquipment(lootId);

		uint iTargetObj;
		bool foundContainer = false;
		pub::SpaceObj::GetTarget(iShip, iTargetObj);

		if (iTargetObj)
		{
			uint objType;
			pub::SpaceObj::GetType(iTargetObj, objType);
			if ((objType & (OBJ_FIGHTER | OBJ_FREIGHTER | OBJ_TRANSPORT | OBJ_GUNBOAT | OBJ_CRUISER | OBJ_CAPITAL | OBJ_DESTROYABLE_DEPOT)) && HkDistance3DByShip(iShip, iTargetObj) < 1000.0f)
			{

				CONTAINER_DATA* container = nullptr;
				if (cd.lastValidTargetId == iTargetObj)
				{
					iSendToClientID = cd.lastValidPlayerId;
				}
				else if (cd.lastValidContainerId == iTargetObj)
				{
					container = cd.lastValidContainer;
				}
				else
				{
					uint iTargetClientID = HkGetClientIDByShip(iTargetObj);
					if (iTargetClientID)
					{
						iSendToClientID = iTargetClientID;
						cd.lastValidTargetId = iTargetObj;
						cd.lastValidPlayerId = iTargetClientID;
					}
					else
					{
						const auto& containerIter = mapMiningContainers.find(iTargetObj);
						if (containerIter != mapMiningContainers.end())
						{
							container = &containerIter->second;
							cd.lastValidContainer = container;
							cd.lastValidContainerId = iTargetObj;
						}
					}
				}

				if (container)
				{
					uint* lootCount = nullptr;
					if (container->loot1Id == lootId)
					{
						foundContainer = true;
						lootCount = &container->loot1Count;
					}
					else if (container->loot2Id == lootId)
					{
						foundContainer = true;
						lootCount = &container->loot2Count;
					}
					if (foundContainer)
					{
						*lootCount += static_cast<uint>(miningYield * set_containerModifier);

						uint amountToJettison = static_cast<uint>(static_cast<float>(set_containerJettisonCount) / lootInfo->fVolume);
						if (*lootCount >= amountToJettison)
						{
							Server.MineAsteroid(container->systemId, container->jettisonPos, set_containerLootCrateID, lootId, amountToJettison, container->clientId);
							*lootCount -= amountToJettison;
						}
					}
				}
			}
		}

		uint miningYieldInt = static_cast<uint>(miningYield);
		cd.overminedFraction = miningYield - miningYieldInt; // save the unused decimal portion for the next mining event.

		if (cd.miningSampleStart < time(nullptr))
		{
			float average = cd.miningEvents / 30.0f;
			if (average > set_miningCheatLogThreshold)
			{
				AddLog("NOTICE: high mining rate charname=%s rate=%0.1f/sec location=%0.0f,%0.0f,%0.0f system=%08x zone=%08x",
					wstos((const wchar_t*)Players.GetActiveCharacterName(iClientID)).c_str(),
					average, vPos.x, vPos.y, vPos.z, zone->iSystemID, zone->iZoneID);
			}

			cd.miningSampleStart = static_cast<uint>(time(nullptr)) + 30;
			cd.miningEvents = 0;
		}

		if (foundContainer)
		{
			return;
		}

		float fHoldRemaining;
		pub::Player::GetRemainingHoldSize(iSendToClientID, fHoldRemaining);
		fHoldRemaining -= cd.equippedVolume;
		if (fHoldRemaining < static_cast<float>(miningYieldInt) * lootInfo->fVolume)
		{
			miningYieldInt = static_cast<uint>(fHoldRemaining / lootInfo->fVolume);
		}

		if (!miningYieldInt)
		{
			if (((uint)time(nullptr) - mapClients[iClientID].LastTimeMessageAboutBeingFull) > 1)
			{
				PrintUserCmdText(iClientID, L"%s's cargo is now full.", reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iSendToClientID)));
				pub::Player::SendNNMessage(iClientID, insufficientCargoSoundId);
				if (iClientID != iSendToClientID)
				{
					PrintUserCmdText(iSendToClientID, L"Your cargo is now full.");
					pub::Player::SendNNMessage(iSendToClientID, insufficientCargoSoundId);
				}
				mapClients[iClientID].LastTimeMessageAboutBeingFull = (uint)time(nullptr);
			}
		}
		else
		{
			pub::Player::AddCargo(iSendToClientID, lootId, miningYieldInt, 1.0, false);
		}
	}

}

void __stdcall MineAsteroid(uint iClientSystemID, class Vector const& vPos, uint iCrateID, uint iLootID, uint iCount, uint iClientID)
{
	returncode = SKIPPLUGINS_NOFUNCTIONCALL;
	CLIENT_DATA& data = mapClients[iClientID];
	if (!data.initialized)
	{
		data.initialized = true;
		CheckClientSetup(iClientID);
	}
	data.itemCount = iCount;
	data.lootID = iLootID;
}

void __stdcall JettisonCargo(unsigned int iClientID, struct XJettisonCargo const& jc)
{
	returncode = DEFAULT_RETURNCODE;
	if (jc.iCount != 1)
	{
		return;
	}

	for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
	{
		if (item->sID != jc.iSlot)
		{
			continue;
		}
		if (item->iArchID != set_deployableContainerCommodity)
		{
			return;
		}
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		const auto& cd = mapClients.find(iClientID);
		if (cd != mapClients.end() && cd->second.deployedContainerId)
		{
			PrintUserCmdText(iClientID, L"ERR A mining container is already deployed");
			return;
		}

		uint shipId;
		uint systemId;
		Vector pos;
		Matrix ori;
		wstring commodityName1;
		wstring commodityName2;
		uint loot1Id = 0;
		uint loot2Id = 0;
		pub::Player::GetShip(iClientID, shipId);
		pub::Player::GetSystem(iClientID, systemId);
		pub::SpaceObj::GetLocation(shipId, pos, ori);
		TranslateX(pos, ori, -400);

		CmnAsteroid::CAsteroidSystem* csys = CmnAsteroid::Find(systemId);
		if (!csys)
		{
			PrintUserCmdText(iClientID, L"ERR Not in a mineable field!");
			return;
		}

		bool alreadyFoundFirstMineable = false;
		// Find asteroid field that matches the best.
		for (CmnAsteroid::CAsteroidField* cfield = csys->FindFirst(); cfield; cfield = csys->FindNext())
		{
			uint tempLootId;
			if (!cfield->near_field(pos))
			{
				continue;
			}
			const Universe::IZone* zone = cfield->get_lootable_zone(pos);
			if (!zone || !zone->lootableZone)
			{
				continue;
			}
			const auto& zoneBonusData = set_mapZoneBonus.find(zone->iZoneID);
			if (zoneBonusData != set_mapZoneBonus.end() && zoneBonusData->second.iReplacementLootID)
			{
				tempLootId = zoneBonusData->second.iReplacementLootID;
			}
			else
			{
				tempLootId = zone->lootableZone->dynamic_loot_commodity;
			}

			const GoodInfo* gi = GoodList::find_by_id(tempLootId);
			if (!alreadyFoundFirstMineable)
			{
				loot1Id = tempLootId;
				alreadyFoundFirstMineable = true;
				commodityName1 = HkGetWStringFromIDS(gi->iIDSName);
			}
			else
			{
				loot2Id = tempLootId;
				commodityName2 = HkGetWStringFromIDS(gi->iIDSName);
				break;
			}

		}

		if (!loot1Id)
		{
			PrintUserCmdText(iClientID, L"ERR Not in a mineable field!");
			return;
		}


		wstring fullContainerName;
		if (loot2Id)
		{
			fullContainerName = commodityName1 + L"/" + commodityName2 + L" Container";
		}
		else
		{
			fullContainerName = commodityName1 + L" Container";
		}

		SPAWN_SOLAR_STRUCT data;
		data.iSystemId = systemId;
		data.pos = pos;
		data.ori = ori;
		data.overwrittenName = fullContainerName;
		data.nickname = "player_mining_container_"+itos(iClientID);
		data.solar_ids = 540999 + iClientID;
		data.solarArchetypeId = set_containerSolarArchetypeID;
		data.loadoutArchetypeId = set_containerLoadoutArchetypeID;

		Plugin_Communication(PLUGIN_MESSAGE::CUSTOM_SPAWN_SOLAR, &data);
		if (data.iSpaceObjId)
		{
			pub::SpaceObj::SetRelativeHealth(data.iSpaceObjId, 1.0f);
			CONTAINER_DATA cd;
			cd.systemId = systemId;
			pos.y -= 30;
			cd.jettisonPos = pos;
			cd.loot1Id = loot1Id;
			cd.loot1Name = commodityName1;
			cd.lootCrate1Id = Archetype::GetEquipment(loot1Id)->get_loot_appearance()->iArchID;
			if (loot2Id)
			{
				cd.loot2Id = loot2Id;
				cd.loot2Name = commodityName2;
				cd.lootCrate2Id = Archetype::GetEquipment(loot2Id)->get_loot_appearance()->iArchID;
			}
			cd.nameIDS = data.solar_ids;
			cd.solarName = data.overwrittenName;
			cd.clientId = iClientID;
			mapMiningContainers[data.iSpaceObjId] = cd;
			mapClients[iClientID].deployedContainerId = data.iSpaceObjId;
			pub::Player::RemoveCargo(iClientID, item->sID, 1);
		}

		return;
	}
}

void __stdcall DisConnect(unsigned int iClientID, enum  EFLConnection state)
{
	returncode = DEFAULT_RETURNCODE;
	DestroyContainer(iClientID);
}

void __stdcall CharacterSelect(struct CHARACTER_ID const& cId, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	DestroyContainer(iClientID);

	for (const auto& container : mapMiningContainers)
	{
		HkChangeIDSString(iClientID, container.second.nameIDS, container.second.solarName);
	}
}

void __stdcall SystemSwitchOut(unsigned int iShip, uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	DestroyContainer(iClientID);
}

void __stdcall BaseEnter(uint base, uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	const auto& clientInfo = mapClients.find(iClientID);
	if (clientInfo == mapClients.end())
	{
		return;
	}
	if (clientInfo->second.deployedContainerId
	&&  mapMiningContainers[clientInfo->second.deployedContainerId].systemId != Players[iClientID].iSystemID)
	{
		DestroyContainer(iClientID);
	}
}

void BaseDestroyed(uint space_obj, uint client)
{
	returncode = DEFAULT_RETURNCODE;
	const auto& i = mapMiningContainers.find(space_obj);
	if (i != mapMiningContainers.end())
	{
		const CONTAINER_DATA& cd = i->second;
		mapClients[cd.clientId].deployedContainerId = 0;
		// container destruction drop all contents as well as 'packed up' container.
		if (cd.loot1Count)
		{
			Server.MineAsteroid(cd.systemId, cd.jettisonPos, set_containerLootCrateID, cd.loot1Id, cd.loot1Count, cd.clientId);
		}
		if (cd.loot2Count)
		{
			Server.MineAsteroid(cd.systemId, cd.jettisonPos, set_containerLootCrateID, cd.loot2Id, cd.loot2Count, cd.clientId);
		}
		Server.MineAsteroid(cd.systemId, cd.jettisonPos, set_containerLootCrateID, set_deployableContainerCommodity, 1, cd.clientId);
		mapMiningContainers.erase(space_obj);
	}
}

void PlayerLaunch_AFTER(uint shipID, uint clientID)
{
	mapClients.erase(clientID);
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
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterSelect, PLUGIN_HkIServerImpl_CharacterSelect, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch_AFTER, PLUGIN_HkIServerImpl_PlayerLaunch_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&DisConnect, PLUGIN_HkIServerImpl_DisConnect, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SystemSwitchOut, PLUGIN_HkIServerImpl_SystemSwitchOutComplete, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseEnter, PLUGIN_HkIServerImpl_BaseEnter_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseDestroyed, PLUGIN_BaseDestroyed, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&MineAsteroid, PLUGIN_HkIServerImpl_MineAsteroid, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SPMunitionCollision, PLUGIN_HkIServerImpl_SPMunitionCollision, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&JettisonCargo, PLUGIN_HkIServerImpl_JettisonCargo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkTimerCheckKick, PLUGIN_HkTimerCheckKick, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	return p_PI;
}
