// Balance Magic for Discovery FLHook
// September 2018 by Kazinsal etc.
//
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

// includes 

// Look, I have to do some crazy shit in this plugin
// If you don't know what this does you probably don't want to look any
// deeper into what this plugin actually does
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>

#ifndef byte
typedef unsigned char byte;
#endif

#include <cstdio>
#include <string>
#include <ctime>
#include <list>
#include <map>
#include <algorithm>
#include <FLHook.h>
#include <plugin.h>

// I regret nothing
#include <WinSock2.h>
#include <Ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")


bool UserCmd_SnacClassic(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);

typedef void(*wprintf_fp)(std::wstring format, ...);
typedef bool(*_UserCmdProc)(uint, const wstring &, const wstring &, const wchar_t*);

struct DamageAdjust {
	float fighter;
	float transport;
	float gunboat;
	float cruiser;
	float battlecruiser;
	float battleship;
};

struct USERCMD
{
	wchar_t* wszCmd;
	_UserCmdProc proc;
	wchar_t* usage;
};

USERCMD UserCmds[] =
{
	{ L"/snacclassic", UserCmd_SnacClassic, L"Usage: /snacclassic" },
};

int set_iPluginDebug = 0;
float set_fFighterOverkillAdjustment = 0.15f;
int iLoadedDamageAdjusts = 0;
int iLoadedArmorScales = 0;

LPTSTR szMailSlotName = TEXT(R"(\\.\mailslot\KillposterMailslot)");
HANDLE hMailSlot = nullptr;

sockaddr_in sockaddrFLHook;
sockaddr_in sockaddrKillposter;
SOCKET sKillposterSocket = NULL;

map<unsigned int, struct DamageAdjust> mapDamageAdjust;
map<unsigned int, float> mapArmorScale;
map<uint, pair<float, float>> healingRates;
list<uint> allowedRepairGuns; // This is the projectile hash, not the gun itself.

/// A return code to indicate to FLHook if we want the hook processing to continue.
PLUGIN_RETURNCODE returncode;

BOOL OpenMailslot(void)
{
	hMailSlot = CreateFile(szMailSlotName, GENERIC_WRITE, FILE_SHARE_READ, static_cast<LPSECURITY_ATTRIBUTES>(nullptr), OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, static_cast<HANDLE>(nullptr));

	if (!hMailSlot)
		return FALSE;

	return TRUE;
}

BOOL WriteMailslot(std::wstring szMessage)
{
	DWORD cbWritten;
	std::string szMessageA = wstos(szMessage);

	if (!hMailSlot)
		return FALSE;

	if (!WriteFile(hMailSlot, szMessageA.c_str(), static_cast<DWORD>(lstrlen(szMessageA.c_str()) + 1) * sizeof(TCHAR), &cbWritten, static_cast<LPOVERLAPPED>(nullptr)))
	{
		CloseHandle(hMailSlot);
		hMailSlot = nullptr;
		return FALSE;
	}

	FlushFileBuffers(hMailSlot);
	return TRUE;
}

BOOL WriteKillposterSocket(std::wstring szMessage)
{
	DWORD cbWritten;
	std::string szMessageA = wstos(szMessage);

	if (!sKillposterSocket)
		return FALSE;

	sendto(sKillposterSocket, szMessageA.c_str(), strlen(szMessageA.c_str()), 0, reinterpret_cast<sockaddr *>(&sockaddrKillposter), sizeof(sockaddrKillposter));
	return TRUE;
}


/// Clear client info when a client connects.
/*void ClearClientInfo(uint iClientID)
{
	mapKillmailData.erase(iClientID);
} */

/// Load the configuration
void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;
	mapDamageAdjust.clear();
	healingRates.clear();
	allowedRepairGuns.clear();
	iLoadedDamageAdjusts = 0;
	iLoadedArmorScales = 0;

	// The path to the configuration file.
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + R"(\flhook_plugins\balancemagic.cfg)";

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
						ConPrint(L"DamageAdjust %s (%u) = %f, %f, %f, %f, %f, %f\n", stows(ini.get_name_ptr()).c_str(), CreateID(ini.get_name_ptr()), \
							stEntry.fighter, stEntry.transport, stEntry.gunboat, stEntry.cruiser, stEntry.battlecruiser, stEntry.battleship);
					++iLoadedDamageAdjusts;
				}
			}
			if (ini.is_header("ArmorScale"))
			{
				while (ini.read_value())
				{
					mapArmorScale[CreateID(ini.get_name_ptr())] = ini.get_value_float(0);
					if (set_iPluginDebug)
						ConPrint(L"ArmorScale %s (%u) = %f\n", stows(ini.get_name_ptr()).c_str(), CreateID(ini.get_name_ptr()), \
							ini.get_value_float(0));
					++iLoadedArmorScales;
				}
			}
		}
		ini.close();
	}

	string scHealingCfgFile = string(szCurDir) + R"(\flhook_plugins\healing_rates.cfg)";
	int modifierCount = 0;
	if (ini.open(scHealingCfgFile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("HealingRates"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("rate"))
					{
						uint shipClass = -1;
						float health_per_shot = 0.01f;
						float multiplier = 0.01f;

						shipClass = ini.get_value_int(0);
						multiplier = ini.get_value_float(1);
						health_per_shot = ini.get_value_float(2);

						if (shipClass == -1 || health_per_shot == 0.01f || multiplier == 0.01f)
						{
							ConPrint(L"BALANCEMAGIC: Invalid repair rate setup.\n");
							continue;
						}

						healingRates[shipClass] = { multiplier, health_per_shot };
						modifierCount++;
					}
				}
			}

			else if (ini.is_header("HealingGuns"))
				while (ini.read_value())
					if (ini.is_value("gun"))
						allowedRepairGuns.emplace_back(CreateID(ini.get_value_string()));
		}
		ini.close();
	}

	ConPrint(L"BALANCEMAGIC: Loaded %u damage adjusts.\n", iLoadedDamageAdjusts);
	ConPrint(L"BALANCEMAGIC: Loaded %u armor scales.\n", iLoadedArmorScales);
	ConPrint(L"BALANCEMAGIC: Loaded %u repair ship class modifiers.\n", modifierCount);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	static bool patched = false;
	srand(static_cast<uint>(time(nullptr)));

	// If we're being loaded from the command line while FLHook is running then
	// set_scCfgFile will not be empty so load the settings as FLHook only
	// calls load settings on FLHook startup and .rehash.
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		if (set_scCfgFile.length() > 0)
			LoadSettings();

		WSAData data;
		WSAStartup(MAKEWORD(2, 2), &data);

		ULONG* srcAddr = new ULONG;
		ULONG* destAddr = new ULONG;

		sockaddrFLHook.sin_family = AF_INET;
		inet_pton(AF_INET, "127.0.0.1", srcAddr);
		sockaddrFLHook.sin_addr.s_addr = *srcAddr;
		sockaddrFLHook.sin_port = htons(0);

		sockaddrKillposter.sin_family = AF_INET;
		inet_pton(AF_INET, "127.0.0.1", destAddr);
		sockaddrKillposter.sin_addr.s_addr = *destAddr;
		sockaddrKillposter.sin_port = htons(31337);

		sKillposterSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		bind(sKillposterSocket, reinterpret_cast<sockaddr *>(&sockaddrFLHook), sizeof(sockaddrFLHook));
	}
	return true;
}

// Command-Option-X-O
bool UserCmd_SnacClassic(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	uint baseID = 0;
	pub::Player::GetBase(iClientID, baseID);
	if (!baseID)
	{
		PrintUserCmdText(iClientID, L"ERR cannot engage time machine while undocked");
		return true;
	}

	int iSNACs = 0;
	int iRemHoldSize;
	list<CARGO_INFO> lstCargo;
	HkEnumCargo(ARG_CLIENTID(iClientID), lstCargo, iRemHoldSize);

	foreach(lstCargo, CARGO_INFO, it)
	{
		if ((*it).bMounted)
			continue;

		if (it->iArchID == CreateID("dsy_snova_civ"))
		{
			iSNACs += it->iCount;
			pub::Player::RemoveCargo(iClientID, it->iID, it->iCount);
		}
	}

	if (iSNACs)
	{
		unsigned int good = CreateID("dsy_snova_classic");
		pub::Player::AddCargo(iClientID, good, iSNACs, 1.0, false);
		PrintUserCmdText(iClientID, L"The time machine ate %i modern-day SNACs and gave back old rusty ones from a bygone era.", iSNACs);
	}
	else
	{
		PrintUserCmdText(iClientID, L"The time machine was disappointed to find you had no unmounted SNACs to relinquish unto it");
	}
	return true;
}

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
				// We handled the command tell FL hook to stop processing this
				// chat string.
				returncode = SKIPPLUGINS_NOFUNCTIONCALL; // we handled the command, return immediatly
				return true;
			}
		}
	}
	return false;
}

bool ExecuteCommandString_Callback(CCmds* cmds, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	if (wscCmd.compare(L"balancemagic"))
		return false;

	if (!cmds->ArgStrToEnd(1).compare(L"reload"))
	{
		cmds->Print(L"BALANCEMAGIC: Live reload requested by %s.\n", cmds->GetAdminName());
		LoadSettings();
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		cmds->Print(L"BALANCEMAGIC: Live reload completed; loaded %u damage adjusts.\n", iLoadedDamageAdjusts);
		return true;
	}
	else
	{
		cmds->Print(L"Usage:\n");
		cmds->Print(L"  .balancemagic reload -- Reloads balancemagic.cfg on the fly.\n");
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		return true;
	}
	return false;
}

// Not needed currently
/*void __stdcall CharacterSelect(struct CHARACTER_ID const &charid, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
}*/

// Not needed currently
/*void __stdcall ReqModifyItem(unsigned short iArchID, char const *Hardpoint, int count, float p4, bool bMounted, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	//PrintUserCmdText(iClientID, L"ReqModifyItem(iArchID = %u, Hardpoint = %s, count = %i, p4 = %f, bMounted = %b, iClientID = %u)", iArchID, Hardpoint, count, p4, bMounted, iClientID);
}*/

void __stdcall HkCb_AddDmgEntry(DamageList *dmg, ushort p1, float damage, enum DamageEntry::SubObjFate fate)
{
	returncode = DEFAULT_RETURNCODE;
	const uint iDmgFrom = HkGetClientIDByShip(dmg->get_inflictor_id());
	if (iDmgToSpaceID && iDmgFrom)
	{
		float curr, max;
		float adjustedDamage = damage;

		if (set_iPluginDebug && p1 != 65521)
			PrintUserCmdText(iDmgFrom, L"Hull hit (p1 = %u).", p1);

		const uint iBeingHitClientID = HkGetClientIDByShip(iDmgToSpaceID); // The person being hit
		if (iBeingHitClientID) // If it's a player 
		{
			Archetype::Ship* TheShipArch = Archetype::GetShip(Players[iDmgFrom].iShipArchetype);  // Get the ship of the person firing
			if (TheShipArch->iShipClass == 19 && find(begin(allowedRepairGuns), end(allowedRepairGuns), iDmgMunitionID) != end(allowedRepairGuns))
			{ // We check whether they are a repair ship (19 is reserved for repair ships), and whether the gun that hit was a valid repair gun
				// We assume no shields
				bool bShieldsUp = false;
				pub::SpaceObj::GetShieldHealth(iDmgToSpaceID, curr, max, bShieldsUp);

				// If their shields are up we don't care 
				if (bShieldsUp)
					return;

				pub::SpaceObj::GetHealth(iDmgToSpaceID, curr, max);
				//PrintUserCmdText(iDmgFrom, L"Current Health: %0f.", curr);
				//PrintUserCmdText(iDmgFrom, L"Max Health: %0f.", max);

				//Handle the healing.
				returncode = SKIPPLUGINS_NOFUNCTIONCALL;

				// Get the ship to heal
				Archetype::Ship* TheShipArchHealed = Archetype::GetShip(Players[iBeingHitClientID].iShipArchetype);

				// If we cannot find a multiplier for this ship class, ignore it
				if (healingRates.find(TheShipArchHealed->iShipClass) == healingRates.end())
					return;

				// Get the amount to heal
				const float fAmountToHeal = max / 100 * healingRates[TheShipArchHealed->iShipClass].first + healingRates[TheShipArchHealed->iShipClass].second;

				// Their health would be above the max value
				if (curr + fAmountToHeal > max)
				{
					//PrintUserCmdText(iDmgFrom, L"You healed the client to full health.");
					dmg->add_damage_entry(1, max, static_cast<DamageEntry::SubObjFate>(0));
					return;
				}

				// Health is less than max value
				//PrintUserCmdText(iDmgFrom, L"You healed the client to %u from %u.", curr + fAmountToHeal, curr);
				dmg->add_damage_entry(1, fAmountToHeal + curr, static_cast<DamageEntry::SubObjFate>(0));
				return;
			}

			else
			{
				map<unsigned int, struct DamageAdjust>::iterator iter = mapDamageAdjust.find(iDmgMunitionID);
				if (iter != mapDamageAdjust.end())
				{
					uint uArchID = 0;
					uint uTargetType = 0;

					pub::SpaceObj::GetSolarArchetypeID(iDmgToSpaceID, uArchID);
					pub::SpaceObj::GetType(iDmgToSpaceID, uTargetType);

					if (uTargetType != OBJ_FIGHTER && uTargetType != OBJ_FREIGHTER)
						return;

					float fTargetArmorScale = 1.0f;

					int iRemHoldSize;
					list<CARGO_INFO> lstCargo;
					HkEnumCargo(ARG_CLIENTID(iBeingHitClientID), lstCargo, iRemHoldSize);

					foreach(lstCargo, CARGO_INFO, it)
					{
						if (!(*it).bMounted)
							continue;

						map<uint, float>::iterator iter = mapArmorScale.find(it->iArchID);
						if (iter != mapArmorScale.end())
						{
							fTargetArmorScale = iter->second;
							break;
						}
					}

					Archetype::Ship* targetShiparch = Archetype::GetShip(uArchID);
					const uint iTargetShipClass = targetShiparch->iShipClass;
					bool bShieldsUp = false;

					if (p1 == 1)
						pub::SpaceObj::GetHealth(iDmgToSpaceID, curr, max);
					else if (p1 == 65521)
						pub::SpaceObj::GetShieldHealth(iDmgToSpaceID, curr, max, bShieldsUp);

					/*PrintUserCmdText(iDmgFrom, L"HkCb_AddDmgEntry CORONA iDmgToSpaceID=%u get_inflictor_id=%u curr=%0.2f max=%0.0f damage=%0.2f cause=%u is_player=%u player_id=%u fate=%u\n",
					iDmgToSpaceID, dmg->get_inflictor_id(), curr, max, damage, dmg->get_cause(), dmg->is_inflictor_a_player(), dmg->get_inflictor_owner_player(), fate);*/

					if (iTargetShipClass < 6 || iTargetShipClass == 19)
						adjustedDamage = curr - iter->second.fighter / (bShieldsUp ? 2 : fTargetArmorScale);
					else if (iTargetShipClass < 11)
						adjustedDamage = curr - iter->second.transport / (bShieldsUp ? 2 : fTargetArmorScale);
					else if (iTargetShipClass < 13)
						adjustedDamage = curr - iter->second.gunboat / (bShieldsUp ? 2 : fTargetArmorScale);
					else if (iTargetShipClass < 15)
						adjustedDamage = curr - iter->second.cruiser / (bShieldsUp ? 2 : fTargetArmorScale);
					else if (iTargetShipClass < 16)
						adjustedDamage = curr - iter->second.battlecruiser / (bShieldsUp ? 2 : fTargetArmorScale);
					else if (iTargetShipClass < 19)
						adjustedDamage = curr - iter->second.battleship / (bShieldsUp ? 2 : fTargetArmorScale);

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

		/*if (p1 != 1 && p1 != 65521)
		{
			if (set_iPluginDebug)
			{
				unsigned int wtf = 0;
				unsigned int iDunno = 0;

				IObjInspectImpl *obj = NULL;
				if (GetShipInspect(iDmgToSpaceID, obj, iDunno))
				{
					if (obj)
					{
						CShip* cship = (CShip*)HkGetEqObjFromObjRW((IObjRW*)obj);
						CEquipManager* eqmanager = (CEquipManager*)((char*)cship + 0xE4);
						CEquip *equip = eqmanager->FindByID(p1);
						PrintUserCmdText(iDmgFrom, L"You hit a ship's possible subobj %u, and I think this is an 0x%08X of type %u.", p1, equip->archetype->iArchID, equip->GetType());
					}
				}
			}
		}*/
	}
}

/* Uneeded currently
void __stdcall SPScanCargo(unsigned int const &iInstigatorShip, unsigned int const &iTargetShip, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;
}*/

// Uneeded currently
/*void HkTimerCheckKick()
{
	returncode = DEFAULT_RETURNCODE;

	uint curr_time = static_cast<uint>(time(nullptr));
	static int messageno = 1;

	wchar_t message[1024];
	swprintf_s(message, L"Hi! I'm an FLHook plugin! This is message number %d.", messageno++);
	//WriteKillposterSocket(message);
} */

void Plugin_Communication_Callback(PLUGIN_MESSAGE msg, void* data)
{
	returncode = DEFAULT_RETURNCODE;

	if (msg == COMBAT_DAMAGE_OVERRIDE)
	{
		returncode = SKIPPLUGINS;
		COMBAT_DAMAGE_OVERRIDE_STRUCT* info = reinterpret_cast<COMBAT_DAMAGE_OVERRIDE_STRUCT*>(data);
		map<unsigned int, struct DamageAdjust>::iterator iter = mapDamageAdjust.find(info->iMunitionID);
		if (iter != mapDamageAdjust.end())
		{
			info->fDamage = iter->second.battleship;
			//ConPrint(L"base: Got a request to override 0x%08X, info.fDamage = %0.0f\n", info->iMunitionID, info->fDamage);
		}
	}
	return;
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
	p_PI->lstHooks.emplace_back(reinterpret_cast<FARPROC*>(&LoadSettings), PLUGIN_LoadSettings, 0);
	p_PI->lstHooks.emplace_back(reinterpret_cast<FARPROC*>(&HkCb_AddDmgEntry), PLUGIN_HkCb_AddDmgEntry, 0);
	p_PI->lstHooks.emplace_back(reinterpret_cast<FARPROC*>(&UserCmd_Process), PLUGIN_UserCmd_Process, 0);
	p_PI->lstHooks.emplace_back(reinterpret_cast<FARPROC*>(&ExecuteCommandString_Callback), PLUGIN_ExecuteCommandString_Callback, 0);
	//p_PI->lstHooks.emplace_back(reinterpret_cast<FARPROC*>(&ShipDestroyed), PLUGIN_ShipDestroyed, 0);
	//p_PI->lstHooks.emplace_back(reinterpret_cast<FARPROC*>(&ClearClientInfo), PLUGIN_ClearClientInfo, 0);
	//p_PI->lstHooks.emplace_back(reinterpret_cast<FARPROC*>(&CharacterSelect), PLUGIN_HkIServerImpl_CharacterSelect, 0);
	//p_PI->lstHooks.emplace_back(reinterpret_cast<FARPROC*>(&ReqModifyItem), PLUGIN_HkIServerImpl_ReqModifyItem, 0);
	//p_PI->lstHooks.emplace_back(reinterpret_cast<FARPROC*>(&SPScanCargo), PLUGIN_HkIServerImpl_SPScanCargo, 0);
	//p_PI->lstHooks.emplace_back(reinterpret_cast<FARPROC*>(&HkTimerCheckKick), PLUGIN_HkTimerCheckKick, 0);
	p_PI->lstHooks.emplace_back(reinterpret_cast<FARPROC*>(&Plugin_Communication_Callback), PLUGIN_Plugin_Communication, 10);
	return p_PI;
}
