// PvE Controller for Discovery FLHook
// April 2020 by Kazinsal etc.
//
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.


#include <windows.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <math.h>
#include <list>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <random>

#include <FLHook.h>
#include <plugin.h>
#include <PluginUtilities.h>

using namespace std;

PLUGIN_RETURNCODE returncode;

#define PLUGIN_DEBUG_NONE 0
#define PLUGIN_DEBUG_CONSOLE 1
#define PLUGIN_DEBUG_VERBOSE 2
#define PLUGIN_DEBUG_VERYVERBOSE 3

struct CLIENT_DATA {
	int bounty_count;
	int bounty_pool;
};

struct stBountyBasePayout {
	int iBasePayout;
};

struct stDropInfo {
	uint uGoodID;
	float fChance;
};

struct stWarzone {
	uint uFaction1;
	uint uFaction2;
	float fMultiplier;
};

CLIENT_DATA aClientData[250];
unordered_map<uint, stBountyBasePayout> mapBountyPayouts;
unordered_map<uint, stBountyBasePayout> mapBountyShipPayouts;
unordered_map<uint, float> mapBountyGroupScale;
unordered_map<uint, float> mapBountyArmorScales;
unordered_map<uint, float> mapBountySystemScales;
multimap<uint, stWarzone> mmapBountyWarzoneScales;

multimap<uint, stDropInfo> mmapDropInfo;
unordered_map<uint, uint> mapShipClassTypes;
map<int, float> mapClassDiffMultipliers;

int set_iPluginDebug = 0;
float set_fMaximumRewardRep = 0.0f;
uint set_uLootCrateID = 0;

bool set_bBountiesEnabled = true;
int set_iPoolPayoutTimer = 0;
int iLoadedNPCBountyClasses = 0;
int iLoadedNPCShipBountyOverrides = 0;
int iLoadedNPCBountyGroupScale = 0;
int iLoadedNPCBountyArmorScales = 0;
int iLoadedNPCBountySystemScales = 0;
int iLoadedClassTypes = 0;
int iLoadedClassDiffMultipliers = 0;
int iLoadedNPCBountyWarzoneScales = 0;
void LoadSettingsNPCBounties(void);


bool set_bDropsEnabled = true;
int iLoadedNPCDropClasses = 0;
void LoadSettingsNPCDrops(void);

/// Clear client info when a client connects.
void ClearClientInfo(uint iClientID)
{
	aClientData[iClientID] = { 0 };
}

/// Load settings.
void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	// The path to the configuration file.
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\pvecontroller.cfg";

	// Load generic settings
	set_iPluginDebug = IniGetI(scPluginCfgFile, "General", "debug", 0);
	set_fMaximumRewardRep = IniGetF(scPluginCfgFile, "General", "maximum_reward_rep", 0.0f);
	set_uLootCrateID = CreateID(IniGetS(scPluginCfgFile, "NPCDrops", "drop_crate", "lootcrate_ast_loot_metal").c_str());

	// Load settings blocks
	LoadSettingsNPCBounties();
	LoadSettingsNPCDrops();
}

void LoadSettingsNPCBounties()
{
	// The path to the configuration file.
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\pvecontroller.cfg";

	// Clear the bounty tables
	mapBountyPayouts.clear();
	iLoadedNPCBountyClasses = 0;
	mapBountyShipPayouts.clear();
	iLoadedNPCShipBountyOverrides = 0;
	mapBountyGroupScale.clear();
	iLoadedNPCBountyGroupScale = 0;
	mapBountyArmorScales.clear();
	iLoadedNPCBountyArmorScales = 0;
	mapBountySystemScales.clear();
	iLoadedNPCBountySystemScales = 0;
	mapShipClassTypes.clear();
	iLoadedClassTypes = 0;
	mapClassDiffMultipliers.clear();
	iLoadedClassDiffMultipliers = 0;
	mmapBountyWarzoneScales.clear();
	iLoadedNPCBountyWarzoneScales = 0;

	// Load ratting bounty settings
	set_iPoolPayoutTimer = IniGetI(scPluginCfgFile, "NPCBounties", "pool_payout_timer", 0);

	// Load the big stuff
	INI_Reader ini;
	if (ini.open(scPluginCfgFile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("NPCBounties"))
			{
				while (ini.read_value())
				{
					if (!strcmp(ini.get_name_ptr(), "enabled"))
					{
						if (ini.get_value_int(0) == 0)
							set_bBountiesEnabled = false;
					}

					if (!strcmp(ini.get_name_ptr(), "group_scale"))
					{
						mapBountyGroupScale[ini.get_value_int(0)] = ini.get_value_float(1);
						++iLoadedNPCBountyGroupScale;
						if (set_iPluginDebug)
							ConPrint(L"PVECONTROLLER: Loaded group scale multiplier %u, %f.\n", ini.get_value_int(0), ini.get_value_float(1));
					}

					if (!strcmp(ini.get_name_ptr(), "class"))
					{
						int iClass = ini.get_value_int(0);
						mapBountyPayouts[iClass].iBasePayout = ini.get_value_int(1);
						++iLoadedNPCBountyClasses;
						if (set_iPluginDebug)
							ConPrint(L"PVECONTROLLER: Loaded class base value %u, $%d.\n", iClass, mapBountyPayouts[iClass].iBasePayout);
					}

					if (!strcmp(ini.get_name_ptr(), "ship"))
					{
						uint uShiparchHash = CreateID(ini.get_value_string(0));
						mapBountyShipPayouts[uShiparchHash].iBasePayout = ini.get_value_int(1);
						++iLoadedNPCShipBountyOverrides;
						if (set_iPluginDebug)
							ConPrint(L"PVECONTROLLER: Loaded override for \"%s\" == %u, $%d.\n", stows(ini.get_value_string(0)).c_str(), uShiparchHash, mapBountyShipPayouts[uShiparchHash].iBasePayout);
					}

					if (!strcmp(ini.get_name_ptr(), "armor_multiplier"))
					{
						uint uArmorHash = CreateID(ini.get_value_string(0));
						mapBountyArmorScales[uArmorHash] = ini.get_value_float(1);
						++iLoadedNPCBountyArmorScales;
						if (set_iPluginDebug)
							ConPrint(L"PVECONTROLLER: Loaded fighter armor multiplier for \"%s\" == %u, %f.\n", stows(ini.get_value_string(0)).c_str(), uArmorHash, ini.get_value_float(1));
					}

					if (!strcmp(ini.get_name_ptr(), "system_multiplier"))
					{
						uint uSystemHash = CreateID(ini.get_value_string(0));
						mapBountySystemScales[uSystemHash] = ini.get_value_float(1);
						++iLoadedNPCBountySystemScales;
						if (set_iPluginDebug)
							ConPrint(L"PVECONTROLLER: Loaded system scale multiplier for \"%s\" == %u, %f.\n", stows(ini.get_value_string(0)).c_str(), uSystemHash, ini.get_value_float(1));
					}

					if (!strcmp(ini.get_name_ptr(), "class_type"))
					{
						for (uint i = 1; i <= ini.get_num_parameters() - 1; i++) {
							mapShipClassTypes[ini.get_value_int(i)] = ini.get_value_int(0);
							++iLoadedClassTypes;
							if (set_iPluginDebug)
								ConPrint(L"PVECONTROLLER: Loaded ship class (%u) as type (%u) \n", ini.get_value_int(i), ini.get_value_int(0));
						}
					}

					if (!strcmp(ini.get_name_ptr(), "class_diff"))
					{
						mapClassDiffMultipliers[ini.get_value_int(0)] = ini.get_value_float(1);
						++iLoadedClassDiffMultipliers;
						if (set_iPluginDebug)
							ConPrint(L"PVECONTROLLER: Loaded class difference multiplier for %i == %f.\n", ini.get_value_int(0), ini.get_value_float(1));
					}

					if (!strcmp(ini.get_name_ptr(), "warzone_multiplier"))
					{
						stWarzone wz;
						uint uSystemHash = CreateID(ini.get_value_string(0));
						uint uFactionHash1 = 0;
						uint uFactionHash2 = 0;
						pub::Reputation::GetReputationGroup(uFactionHash1, ini.get_value_string(1));
						pub::Reputation::GetReputationGroup(uFactionHash2, ini.get_value_string(2));
						wz.uFaction1 = uFactionHash1;
						wz.uFaction2 = uFactionHash2;
						wz.fMultiplier = ini.get_value_float(3);
						mmapBountyWarzoneScales.insert(make_pair(uSystemHash, wz));
						++iLoadedNPCBountyWarzoneScales;
						if (set_iPluginDebug)
							ConPrint(L"PVECONTROLLER: Loaded warzone scale multiplier for \"%s\" == %u, %f.\n", stows(ini.get_value_string(0)).c_str(), uSystemHash, ini.get_value_float(1));
					}
				}
			}

		}
		ini.close();
	}

	ConPrint(L"PVECONTROLLER: NPC bounties are %s.\n", set_bBountiesEnabled ? L"enabled" : L"disabled");
	ConPrint(L"PVECONTROLLER: Loaded %u NPC bounty group scale values.\n", iLoadedNPCBountyGroupScale);
	ConPrint(L"PVECONTROLLER: Loaded %u NPC bounty classes.\n", iLoadedNPCBountyClasses);
	ConPrint(L"PVECONTROLLER: Loaded %u NPC bounty ship overrides.\n", iLoadedNPCShipBountyOverrides);
	ConPrint(L"PVECONTROLLER: Loaded %u NPC bounty fighter armor multipliers.\n", iLoadedNPCBountyArmorScales);
	ConPrint(L"PVECONTROLLER: Loaded %u NPC bounty system scale multipliers.\n", iLoadedNPCBountySystemScales);
	ConPrint(L"PVECONTROLLER: Loaded %u ship class types.\n", iLoadedClassTypes);
	ConPrint(L"PVECONTROLLER: Loaded %u NPC bounty class difference multipliers.\n", iLoadedClassDiffMultipliers);
	ConPrint(L"PVECONTROLLER: Loaded %u NPC bounty warzone scale multipliers.\n", iLoadedNPCBountyWarzoneScales);
}

void LoadSettingsNPCDrops()
{
	// The path to the configuration file.
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\pvecontroller.cfg";

	// Clear the drop tables.
	mmapDropInfo.clear();
	iLoadedNPCDropClasses = 0;

	// Load the big stuff
	INI_Reader ini;
	if (ini.open(scPluginCfgFile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("NPCDrops"))
			{
				while (ini.read_value())
				{
					if (!strcmp(ini.get_name_ptr(), "enabled"))
					{
						if (ini.get_value_int(0) == 0)
							set_bDropsEnabled = false;
					}

					if (!strcmp(ini.get_name_ptr(), "class"))
					{
						stDropInfo drop;
						int iClass = ini.get_value_int(0);
						string szGood = ini.get_value_string(1);
						drop.uGoodID = CreateID(szGood.c_str());
						drop.fChance = ini.get_value_float(2);
						mmapDropInfo.insert(make_pair(iClass, drop));
						++iLoadedNPCDropClasses;
						if (set_iPluginDebug)
							ConPrint(L"PVECONTROLLER: Loaded class %u drop %s (0x%08X), %f chance.\n", iClass, stows(szGood).c_str(), CreateID(szGood.c_str()), drop.fChance);
					}
				}
			}

		}
		ini.close();
	}

	ConPrint(L"PVECONTROLLER: NPC drops are %s.\n", set_bDropsEnabled ? L"enabled" : L"disabled");
	ConPrint(L"PVECONTROLLER: Loaded %u NPC drops by class.\n", iLoadedNPCDropClasses);
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	srand(static_cast<uint>(time(nullptr)));
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


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NPCBountyAddToPool(uint iClientID, int iBounty, bool bNotify) {
	if (!iClientID)
		return;

	aClientData[iClientID].bounty_count++;
	aClientData[iClientID].bounty_pool += iBounty;
	if (bNotify)
		PrintUserCmdText(iClientID, L"A $%s credit bounty has been added to your reward pool.", ToMoneyStr(iBounty).c_str());
}

void NPCBountyPayout(uint iClientID) {
	if (!iClientID)
		return;

	float fValue;
	pub::Player::GetAssetValue(iClientID, fValue);

	int iCurrMoney;
	pub::Player::InspectCash(iClientID, iCurrMoney);

	long long lNewMoney = iCurrMoney;
	lNewMoney += aClientData[iClientID].bounty_pool;

	if (fValue + aClientData[iClientID].bounty_pool > 2000000000 || lNewMoney > 2000000000)
	{
		PrintUserCmdText(iClientID, L"A bounty pool worth $%s credits was attempted to be paid, but the result would overfill your neural net account.", ToMoneyStr(aClientData[iClientID].bounty_pool).c_str());
		PrintUserCmdText(iClientID, L"Payment of this bounty pool will be retried later.");
		return;
	}

	HkAddCash((const wchar_t*)Players.GetActiveCharacterName(iClientID), aClientData[iClientID].bounty_pool);
	PrintUserCmdText(iClientID, L"A bounty pool worth $%s credits for %d kill%s has been deposited in your account.", ToMoneyStr(aClientData[iClientID].bounty_pool).c_str(), aClientData[iClientID].bounty_count, (aClientData[iClientID].bounty_count == 1 ? L"" : L"s"));

	aClientData[iClientID].bounty_count = 0;
	aClientData[iClientID].bounty_pool = 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Command Functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UserCmd_Pool(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (set_iPoolPayoutTimer == 0) {
		PrintUserCmdText(iClientID, L"Bounty pool has been disabled; all bounties are paid out at time of kill.");
		return true;
	}

	uint next_tick = set_iPoolPayoutTimer - ((uint)time(0) % set_iPoolPayoutTimer);
	if (aClientData[iClientID].bounty_pool == 0)
		PrintUserCmdText(iClientID, L"You do not currently have any outstanding bounty payments.");
	else
		PrintUserCmdText(iClientID, L"You will be paid out $%s credits for %d kill%s in %dm%ds.", ToMoneyStr(aClientData[iClientID].bounty_pool).c_str(), aClientData[iClientID].bounty_count, (aClientData[iClientID].bounty_count == 1 ? L"" : L"s"), next_tick / 60, next_tick % 60);

	return true;
}

bool UserCmd_Value(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	float fShipValue = 0;
	HKGetShipValue((const wchar_t*)Players.GetActiveCharacterName(iClientID), fShipValue);
	PrintUserCmdText(iClientID, L"Ship value: $%s credits.", ToMoneyStr(fShipValue).c_str());

	return true;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Client command processing
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef bool(*_UserCmdProc)(uint, const wstring &, const wstring &, const wchar_t*);

struct USERCMD
{
	wchar_t *wszCmd;
	_UserCmdProc proc;
	wchar_t *usage;
};

USERCMD UserCmds[] =
{
	{ L"/pool", UserCmd_Pool, L"" },
	{ L"/value", UserCmd_Value, L"" },
};

/**
This function is called by FLHook when a user types a chat string. We look at the
string they've typed and see if it starts with one of the above commands. If it
does we try to process it.
*/
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
				returncode = SKIPPLUGINS_NOFUNCTIONCALL;
				return true;
			}
		}
	}
	return false;
}

bool ExecuteCommandString_Callback(CCmds* cmds, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	if (wscCmd.compare(L"pvecontroller"))
		return false;

	if (!(cmds->rights & RIGHT_PLUGINS)) { cmds->Print(L"ERR No permission\n"); return false; }

	if (!cmds->ArgStrToEnd(1).compare(L"status"))
	{
		cmds->Print(L"PVECONTROLLER: PvE Controller (Phase 1) is active.\n");
		if (set_iPoolPayoutTimer)
		{
			uint next_tick = set_iPoolPayoutTimer - ((uint)time(0) % set_iPoolPayoutTimer);
			uint pools = 0, poolkills = 0;
			uint64_t poolvalue = 0;
			for (int i = 0; i < 250; i++) {
				if (aClientData[i].bounty_pool) {
					pools++;
					poolvalue += aClientData[i].bounty_pool;
					poolkills += aClientData[i].bounty_count;
				}
			}
			cmds->Print(L"  There are %d outstanding bounty pools worth $%lld credits for %d kill%s to be paid out in %dm%ds.\n", pools, poolvalue, poolkills, (poolkills != 1 ? L"s" : L""), next_tick / 60, next_tick % 60);
		}
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		return true;
	}
	else if (!cmds->ArgStrToEnd(1).compare(L"payout"))
	{
		if (set_iPoolPayoutTimer)
		{
			uint pools = 0, poolkills = 0;
			uint64_t poolvalue = 0;
			for (int i = 0; i < 250; i++) {
				if (aClientData[i].bounty_pool) {
					pools++;
					poolvalue += aClientData[i].bounty_pool;
					poolkills += aClientData[i].bounty_count;
					NPCBountyPayout(i);
				}
			}
			cmds->Print(L"PVECONTROLLER: Paid out %d outstanding bounty pools worth $%lld credits for %d kill%s.\n", pools, poolvalue, poolkills, (poolkills != 1 ? L"s" : L""));
		}
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		return true;
	}
	else if (!cmds->ArgStrToEnd(1).compare(L"reloadall"))
	{
		cmds->Print(L"PVECONTROLLER: COMPLETE LIVE RELOAD requested by %s.\n", cmds->GetAdminName());
		LoadSettings();
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		cmds->Print(L"PVECONTROLLER: Live reload completed.\n");
		return true;
	}
	else if (!cmds->ArgStrToEnd(1).compare(L"reloadnpcbounties"))
	{
		cmds->Print(L"PVECONTROLLER: Live NPC bounties reload requested by %s.\n", cmds->GetAdminName());
		LoadSettingsNPCBounties();
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		cmds->Print(L"PVECONTROLLER: Live NPC bounties reload completed.\n");
		return true;
	}
	else if (!cmds->ArgStrToEnd(1).compare(L"reloadnpcdrops"))
	{
		cmds->Print(L"PVECONTROLLER: Live NPC drops reload requested by %s.\n", cmds->GetAdminName());
		LoadSettingsNPCDrops();
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		cmds->Print(L"PVECONTROLLER: Live NPC drops reload completed.\n");
		return true;
	}
	else
	{
		cmds->Print(L"Usage:\n");
		cmds->Print(L"  .pvecontroller status    -- Displays PvE controller status information.\n");
		cmds->Print(L"  .pvecontroller payout    -- Pays out all outstanding NPC bounties.\n");
		cmds->Print(L"  .pvecontroller reloadall -- Reloads ALL settings on the fly.\n");
		cmds->Print(L"  .pvecontroller reloadnpcbounties -- Reloads NPC bounty settings on the fly.\n");
		cmds->Print(L"  .pvecontroller reloadnpcdrops -- Reloads NPC drop settings on the fly.\n");
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions to hook
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall HkCb_ShipDestroyed(DamageList* dmg, DWORD* ecx, uint iKill)
{
	returncode = DEFAULT_RETURNCODE;

	if (!iKill)
		return;
	CShip* cship = (CShip*)ecx[4];
	if (cship->GetOwnerPlayer())
		return;
	uint iVictimShipId = cship->iSpaceID;

	uint iKillerClientId = HkGetClientIDByShip(reinterpret_cast<uint*>(dmg)[2]); // whatever the first parameter is, it's NOT a DamageList*, but third element is the killer's spaceObjId

	if (!iVictimShipId || !iKillerClientId)
		return;

	if (HkGetClientIDByShip(iVictimShipId))
		return;

	uint iTargetType;
	pub::SpaceObj::GetType(iVictimShipId, iTargetType);

	unsigned int uArchID = 0;
	pub::SpaceObj::GetSolarArchetypeID(iVictimShipId, uArchID);
	Archetype::Ship* victimShiparch = Archetype::GetShip(uArchID);
	if (set_iPluginDebug >= PLUGIN_DEBUG_VERYVERBOSE)
		PrintUserCmdText(iKillerClientId, L"PVECONTROLLER: You killed an NPC uArchID == %u", uArchID);

	// Grab some info we'll need later.
	uint uKillerSystem = 0;
	unsigned int uKillerAffiliation = 0;
	pub::Player::GetSystem(iKillerClientId, uKillerSystem);

	// Deny bounties and drops for kills on targets above the maximum reward reputation threshold.
	int iTargetRep, iPlayerRep;
	uint uTargetAffiliation;
	float fAttitude = 0.0f;
	pub::SpaceObj::GetRep(iVictimShipId, iTargetRep);
	Reputation::Vibe::GetAffiliation(iTargetRep, uTargetAffiliation, false);
	pub::SpaceObj::GetRep(dmg->get_inflictor_id(), iPlayerRep);
	Reputation::Vibe::Verify(iPlayerRep);
	Reputation::Vibe::GetAffiliation(iPlayerRep, uKillerAffiliation, false);
	pub::Reputation::GetGroupFeelingsTowards(iPlayerRep, uTargetAffiliation, fAttitude);
	if (fAttitude > set_fMaximumRewardRep) {
		if (set_bBountiesEnabled)
			PrintUserCmdText(iKillerClientId, L"Can not pay bounty against ineligible combatant (reputation towards target must be %0.2f or lower).", set_fMaximumRewardRep);
		return;
	}

	// Process bounties if enabled.
	if (set_bBountiesEnabled) {
		int iBountyPayout = 0;

		// Determine bounty payout.
		const auto& iter = mapBountyShipPayouts.find(uArchID);
		if (iter != mapBountyShipPayouts.end()) {
			if (set_iPluginDebug >= PLUGIN_DEBUG_VERBOSE)
				PrintUserCmdText(iKillerClientId, L"Overriding payout for uarch %u to be $%d.", uArchID, iter->second.iBasePayout);
			iBountyPayout = iter->second.iBasePayout;
		}
		else {
			const auto& iter = mapBountyPayouts.find(victimShiparch->iShipClass);
			if (iter != mapBountyPayouts.end()) {
				iBountyPayout = iter->second.iBasePayout;
				if (victimShiparch->iShipClass < 5) {
					unsigned int iDunno = 0;
					IObjInspectImpl* obj = NULL;
					if (GetShipInspect(iVictimShipId, obj, iDunno)) {
						if (obj) {
							CShip* cship = (CShip*)HkGetEqObjFromObjRW((IObjRW*)obj);
							CEquipManager* eqmanager = (CEquipManager*)((char*)cship + 0xE4);
							CEArmor* cearmor = (CEArmor*)eqmanager->FindFirst(0x1000000);

							// If the NPC has armour, see if we have an armour scale multiplier to use on it.
							if (cearmor) {
								const auto& iter = mapBountyArmorScales.find(cearmor->archetype->iArchID);
								if (iter != mapBountyArmorScales.end())
									iBountyPayout = (int)((float)iBountyPayout * iter->second);
							}
						}
					}
				}
			}
		}

		if (iLoadedNPCBountyWarzoneScales) {
			const auto& iter = mmapBountyWarzoneScales.find(uKillerSystem);
			if (iter != mmapBountyWarzoneScales.end())
			{
				if ((iter->second.uFaction1 == uKillerAffiliation && iter->second.uFaction2 == uTargetAffiliation) || (iter->second.uFaction2 == uKillerAffiliation && iter->second.uFaction1 == uTargetAffiliation)) {
					if (set_iPluginDebug >= PLUGIN_DEBUG_VERYVERBOSE)
						PrintUserCmdText(iKillerClientId, L"PVECONTROLLER: Killer (%u) and Target (%u) have valid warzone multipliyer of %0.2f", uKillerAffiliation, uTargetAffiliation, iter->second.fMultiplier);
					iBountyPayout *= iter->second.fMultiplier;
				}
			}
		}

		// Multiply by system multiplier if applicable.
		if (iLoadedNPCBountySystemScales) {
			const auto& itSystemScale = mapBountySystemScales.find(uKillerSystem);
			if (itSystemScale != mapBountySystemScales.end())
				iBountyPayout *= itSystemScale->second;
		}

		// Multiply by class diff multiplier if applicable.
		if (iLoadedClassDiffMultipliers) {
			uint iKillerShipClass = Archetype::GetShip(Players[iKillerClientId].iShipArchetype)->iShipClass;

			int classDiff = 0;
			const auto& itVictimType = mapShipClassTypes.find(victimShiparch->iShipClass);
			const auto& itKillerType = mapShipClassTypes.find(iKillerShipClass);
			if (itVictimType != mapShipClassTypes.end() && itKillerType != mapShipClassTypes.end())
				classDiff = itVictimType->second - itKillerType->second;

			const auto& itDiffMultiplier = mapClassDiffMultipliers.lower_bound(classDiff);
			if (itDiffMultiplier != mapClassDiffMultipliers.end())
				iBountyPayout *= itDiffMultiplier->second;
			if (set_iPluginDebug >= PLUGIN_DEBUG_VERYVERBOSE)
				PrintUserCmdText(iKillerClientId, L"PVECONTROLLER: Modifying payout to $%d (%0.2f x normal) due to class difference. %u vs %u \n", iBountyPayout, itDiffMultiplier->second, itKillerType->second, itVictimType->second);
		}

		// If we've turned bounties off, don't pay it.
		if (!set_bBountiesEnabled)
			iBountyPayout = 0;

		if (iBountyPayout) {
			list<GROUP_MEMBER> lstMembers;
			HkGetGroupMembers((const wchar_t*)Players.GetActiveCharacterName(iKillerClientId), lstMembers);

			if (set_iPluginDebug >= PLUGIN_DEBUG_VERYVERBOSE)
				PrintUserCmdText(iKillerClientId, L"PVECONTROLLER: There are %u players in your group.", lstMembers.size());

			foreach(lstMembers, GROUP_MEMBER, gm) {
				uint uGroupMemberSystem = 0;
				pub::Player::GetSystem(gm->iClientID, uGroupMemberSystem);
				if (uKillerSystem != uGroupMemberSystem)
					lstMembers.erase(gm);
			}

			if (mapBountyGroupScale.count(lstMembers.size()))
				iBountyPayout = (int)((float)iBountyPayout * mapBountyGroupScale[lstMembers.size()]);
			else
				iBountyPayout = (int)((float)iBountyPayout / lstMembers.size());

			if (set_iPluginDebug >= PLUGIN_DEBUG_VERYVERBOSE)
				PrintUserCmdText(iKillerClientId, L"PVECONTROLLER: Paying out $%d to %u eligible group members in your system.", iBountyPayout, lstMembers.size());

			foreach(lstMembers, GROUP_MEMBER, gm) {
				NPCBountyAddToPool(gm->iClientID, iBountyPayout, set_iPoolPayoutTimer);
				if (!set_iPoolPayoutTimer)
					NPCBountyPayout(gm->iClientID);
			}

		}
	}

	// Process drops if enabled.
	if (set_bDropsEnabled) {
		const auto& iter = mmapDropInfo.find(victimShiparch->iShipClass);
		if (iter != mmapDropInfo.end())
		{
			if (set_iPluginDebug >= PLUGIN_DEBUG_VERYVERBOSE)
				PrintUserCmdText(iKillerClientId, L"PVECONTROLLER: class %d drop entry found, %f chance to drop 0x%08X.\n", iter->first, iter->second.fChance, iter->second.uGoodID);
			float roll = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
			if (roll < iter->second.fChance) {
				if (set_iPluginDebug >= PLUGIN_DEBUG_VERYVERBOSE)
					PrintUserCmdText(iKillerClientId, L"PVECONTROLLER: Rolled %f, won a drop!\n", roll);

				Vector vLoc = { 0.0f, 0.0f, 0.0f };
				Matrix mRot = { 0.0f, 0.0f, 0.0f };
				pub::SpaceObj::GetLocation(iVictimShipId, vLoc, mRot);
				vLoc.x += 30.0;
				Server.MineAsteroid(uKillerSystem, vLoc, set_uLootCrateID, iter->second.uGoodID, 1, iKillerClientId);
			}
			else
				if (set_iPluginDebug >= PLUGIN_DEBUG_VERYVERBOSE)
					PrintUserCmdText(iKillerClientId, L"PVECONTROLLER: Rolled %f, no drop for you.\n", roll);
		}
	}
}

void HkTimerCheckKick()
{
	returncode = DEFAULT_RETURNCODE;
	uint curr_time = (uint)time(0);

	// Pay bounty pools as required.
	if (set_iPoolPayoutTimer) {
		if (curr_time % set_iPoolPayoutTimer == 0) {
			for (int i = 0; i < 250; i++) {
				if (aClientData[i].bounty_pool)
					NPCBountyPayout(i);
			}
		}
	}
}

void __stdcall BaseEnter(uint iBaseID, uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	// Pay bounty pool when a client docks so we don't have to screw around with client disconnections and other garbage like that.
	if (aClientData[iClientID].bounty_pool)
		NPCBountyPayout(iClientID);
}

void __stdcall DisConnect(uint iClientID, enum EFLConnection p2)
{
	returncode = DEFAULT_RETURNCODE;

	//ConPrint(L"PVE: DisConnect for id=%d char=%s\n", iClientID, Players.GetActiveCharacterName(iClientID));

	/*if (ClientInfo[iClientID].bCharSelected)
		NPCBountyPayout(iClientID);*/

	ClearClientInfo(iClientID);
}

EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "PvE Controller by Kazinsal et al.";
	p_PI->sShortName = "pvecontroller";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;
	
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ClearClientInfo, PLUGIN_ClearClientInfo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ExecuteCommandString_Callback, PLUGIN_ExecuteCommandString_Callback, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkCb_ShipDestroyed, PLUGIN_ShipDestroyed, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkTimerCheckKick, PLUGIN_HkTimerCheckKick, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseEnter, PLUGIN_HkIServerImpl_BaseEnter, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&DisConnect, PLUGIN_HkIServerImpl_DisConnect, 0));

	return p_PI;
}
