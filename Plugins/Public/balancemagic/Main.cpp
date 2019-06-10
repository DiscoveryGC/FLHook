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

#include <windows.h>

#ifndef byte
typedef unsigned char byte;
#endif

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
#include <PluginUtilities.h>

// I regret nothing
#include <WinSock2.h>
#include <Ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")


bool UserCmd_SnacClassic(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);

typedef void(*wprintf_fp)(std::wstring format, ...);
typedef bool(*_UserCmdProc)(uint, const wstring &, const wstring &, const wchar_t*);

struct DamageAdjust {
	float fighter;
	float freighter;
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

struct AmmoGenerator
{
	unsigned int ammo;
	unsigned int count;
	mstime timestamp;
};

struct CLIENT_DATA
{
	map<ushort, AmmoGenerator> mapBurstFills;
};

USERCMD UserCmds[] =
{
	{ L"/snacclassic", UserCmd_SnacClassic, L"Usage: /snacclassic" },
};

int set_iPluginDebug = 0;
float set_fFighterOverkillAdjustment = 0.15f;
int iLoadedDamageAdjusts = 0;
int iLoadedArmorScales = 0;
int iLoadedBurstFires = 0;

LPTSTR szMailSlotName = TEXT("\\\\.\\mailslot\\KillposterMailslot");
HANDLE hMailSlot = NULL;

sockaddr_in sockaddrFLHook;
sockaddr_in sockaddrKillposter;
SOCKET sKillposterSocket = NULL;

CLIENT_DATA aClientData[250];

map<unsigned int, struct DamageAdjust> mapDamageAdjust;
map<unsigned int, float> mapArmorScale;
map<unsigned int, struct AmmoGenerator> mapBurstFire;

/// A return code to indicate to FLHook if we want the hook processing to continue.
PLUGIN_RETURNCODE returncode;

BOOL OpenMailslot(void)
{
	hMailSlot = CreateFile(szMailSlotName, GENERIC_WRITE, FILE_SHARE_READ, (LPSECURITY_ATTRIBUTES)NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL);

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

	if (!WriteFile(hMailSlot, szMessageA.c_str(), (DWORD)(lstrlen(szMessageA.c_str()) + 1) * sizeof(TCHAR), &cbWritten, (LPOVERLAPPED)NULL))
	{
		CloseHandle(hMailSlot);
		hMailSlot = false;
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

	sendto(sKillposterSocket, szMessageA.c_str(), strlen(szMessageA.c_str()), 0, (sockaddr *)&sockaddrKillposter, sizeof(sockaddrKillposter));
	return TRUE;
}


/// Clear client info when a client connects.
void ClearClientInfo(uint iClientID)
{
	ConPrint(L"BURSTFIRE: Clearing client info for client %u... ", iClientID);
	aClientData[iClientID].mapBurstFills.clear();
	ConPrint(L"cleared.\n", iClientID);
}

/// Load the configuration
void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;
	mapDamageAdjust.clear();
	iLoadedDamageAdjusts = 0;
	iLoadedArmorScales = 0;
	iLoadedBurstFires = 0;

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
					stEntry.freighter = ini.get_value_float(1) ? ini.get_value_float(1) : stEntry.fighter;
					stEntry.transport = ini.get_value_float(2) ? ini.get_value_float(2) : stEntry.freighter;
					stEntry.gunboat = ini.get_value_float(3) ? ini.get_value_float(3) : stEntry.transport;
					stEntry.cruiser = ini.get_value_float(4) ? ini.get_value_float(4) : stEntry.gunboat;
					stEntry.battlecruiser = ini.get_value_float(5) ? ini.get_value_float(5) : stEntry.cruiser;
					stEntry.battleship = ini.get_value_float(6) ? ini.get_value_float(6) : stEntry.battlecruiser;
					mapDamageAdjust[CreateID(ini.get_name_ptr())] = stEntry;
					if (set_iPluginDebug)
						ConPrint(L"DamageAdjust %s (%u) = %f, %f, %f, %f, %f, %f, %f\n", stows(ini.get_name_ptr()).c_str(), CreateID(ini.get_name_ptr()), \
							stEntry.fighter, stEntry.freighter, stEntry.transport, stEntry.gunboat, stEntry.cruiser, stEntry.battlecruiser, stEntry.battleship);
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
			if (ini.is_header("BurstFire"))
			{
				while (ini.read_value())
				{
					struct AmmoGenerator stEntry;
					stEntry.ammo = CreateID(ini.get_value_string(0));
					stEntry.count = ini.get_value_int(1);
					stEntry.timestamp = ini.get_value_int(2);
					mapBurstFire[CreateID(ini.get_name_ptr())] = stEntry;
					if (set_iPluginDebug)
						ConPrint(L"BurstFire %s (%u) = %d x %s after %llu ms\n", stows(ini.get_name_ptr()).c_str(), CreateID(ini.get_name_ptr()), \
							ini.get_value_string(0), stEntry.count, stEntry.timestamp);
					++iLoadedBurstFires;
				}
			}
		}
		ini.close();
	}

	ConPrint(L"BALANCEMAGIC: Loaded %u damage adjusts.\n", iLoadedDamageAdjusts);
	ConPrint(L"BALANCEMAGIC: Loaded %u armor scales.\n", iLoadedArmorScales);
	ConPrint(L"BALANCEMAGIC: Loaded %u burst fire weapons.\n", iLoadedBurstFires);
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
		bind(sKillposterSocket, (sockaddr *)&sockaddrFLHook, sizeof(sockaddrFLHook));
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

void __stdcall CharacterSelect(struct CHARACTER_ID const &charid, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
}

void __stdcall ReqModifyItem(unsigned short iArchID, char const *Hardpoint, int count, float p4, bool bMounted, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	//PrintUserCmdText(iClientID, L"ReqModifyItem(iArchID = %u, Hardpoint = %s, count = %i, p4 = %f, bMounted = %b, iClientID = %u)", iArchID, Hardpoint, count, p4, bMounted, iClientID);
}

void __stdcall HkCb_AddDmgEntry(DamageList *dmg, unsigned short p1, float damage, enum DamageEntry::SubObjFate fate)
{
	returncode = DEFAULT_RETURNCODE;
	uint iDmgFrom = HkGetClientIDByShip(dmg->get_inflictor_id());
	if (iDmgToSpaceID && iDmgFrom)
	{
		float curr, max;
		float adjustedDamage = damage;
		
		if (set_iPluginDebug && p1 != 65521)
			PrintUserCmdText(iDmgFrom, L"Hull hit (p1 = %u).", p1);

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

		map<unsigned int, struct DamageAdjust>::iterator iter = mapDamageAdjust.find(iDmgMunitionID);
		if (iter != mapDamageAdjust.end())
		{
			unsigned int uArchID = 0;
			unsigned int uTargetType = 0;

			pub::SpaceObj::GetSolarArchetypeID(iDmgToSpaceID, uArchID);
			pub::SpaceObj::GetType(iDmgToSpaceID, uTargetType);

			if (uTargetType != OBJ_FIGHTER && uTargetType != OBJ_FREIGHTER)
				return;

			float fTargetArmorScale = 1.0f;

			if (HkGetClientIDByShip(iDmgToSpaceID))
			{
				int iRemHoldSize;
				list<CARGO_INFO> lstCargo;
				HkEnumCargo(ARG_CLIENTID(HkGetClientIDByShip(iDmgToSpaceID)), lstCargo, iRemHoldSize);

				foreach(lstCargo, CARGO_INFO, it)
				{
					if (!(*it).bMounted)
						continue;

					map<unsigned int, float>::iterator iter = mapArmorScale.find(it->iArchID);
					if (iter != mapArmorScale.end())
					{
						fTargetArmorScale = iter->second;
						break;
					}
				}
			}

			Archetype::Ship* targetShiparch = Archetype::GetShip(uArchID);
			uint targetShipClass = targetShiparch->iShipClass;
			bool bShieldsUp = false;

			if (p1 == 1)
				pub::SpaceObj::GetHealth(iDmgToSpaceID, curr, max);
			else if (p1 == 65521)
				pub::SpaceObj::GetShieldHealth(iDmgToSpaceID, curr, max, bShieldsUp);

			/*PrintUserCmdText(iDmgFrom, L"HkCb_AddDmgEntry CORONA iDmgToSpaceID=%u get_inflictor_id=%u curr=%0.2f max=%0.0f damage=%0.2f cause=%u is_player=%u player_id=%u fate=%u\n",
			iDmgToSpaceID, dmg->get_inflictor_id(), curr, max, damage, dmg->get_cause(), dmg->is_inflictor_a_player(), dmg->get_inflictor_owner_player(), fate);*/

			if (targetShipClass == 0 || targetShipClass == 1 || targetShipClass == 3)
				adjustedDamage = curr - iter->second.fighter / (bShieldsUp ? 2 : fTargetArmorScale);
			else if (targetShipClass == 2 || targetShipClass == 4 || targetShipClass == 5 || targetShipClass == 19)
				adjustedDamage = curr - iter->second.freighter / (bShieldsUp ? 2 : fTargetArmorScale);
			else if (targetShipClass < 11)
				adjustedDamage = curr - iter->second.transport / (bShieldsUp ? 2 : fTargetArmorScale);
			else if (targetShipClass < 13)
				adjustedDamage = curr - iter->second.gunboat / (bShieldsUp ? 2 : fTargetArmorScale);
			else if (targetShipClass < 15)
				adjustedDamage = curr - iter->second.cruiser / (bShieldsUp ? 2 : fTargetArmorScale);
			else if (targetShipClass < 16)
				adjustedDamage = curr - iter->second.battlecruiser / (bShieldsUp ? 2 : fTargetArmorScale);
			else if (targetShipClass < 19)
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

void __stdcall FireWeapon(unsigned int iClientID, struct XFireWeaponInfo const &wpn)
{
	returncode = DEFAULT_RETURNCODE;

	if (!iLoadedBurstFires)
		return;

	IObjInspectImpl *obj = HkGetInspect(iClientID);
	if (obj)
	{
		CShip* cship = (CShip*)HkGetEqObjFromObjRW((IObjRW*)obj);

		for (short* slot = wpn.sArray1; slot != wpn.sArray2; slot++)
		{
			CEquipManager* eqmanager = (CEquipManager*)((char*)cship + 0xE4);
			CEquip *equip = eqmanager->FindByID(*slot);
			if (CEGun::cast(equip))
			{
				CEGun* cgun = (CEGun*)equip;
				map<unsigned int, struct AmmoGenerator>::iterator iter = mapBurstFire.find(cgun->GunArch()->iArchID);
				if (iter != mapBurstFire.end())
				{
					aClientData[iClientID].mapBurstFills[*slot].ammo = iter->second.ammo;
					aClientData[iClientID].mapBurstFills[*slot].count++;
					aClientData[iClientID].mapBurstFills[*slot].timestamp = GetTimeInMS() + iter->second.timestamp;
					ConPrint(L"BURSTFIRE: Client %u fired a round of 0x%08X from slot %u.\n", iClientID, iter->second.ammo, *slot);
				}
				//PrintUserCmdText(iClientID, L"FireWeapon: 0x%08X", cgun->GunArch()->iArchID);
			}
		}
	}
}

void __stdcall SPScanCargo(unsigned int const &iInstigatorShip, unsigned int const &iTargetShip, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;
}

void __stdcall PlayerLaunch_AFTER(unsigned int iShipID, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	if (!iLoadedBurstFires)
		return;

	aClientData[iClientID].mapBurstFills.clear();
	int i = 0;

	IObjInspectImpl *obj = HkGetInspect(iClientID);
	if (obj)
	{
		CShip* cship = (CShip*)HkGetEqObjFromObjRW((IObjRW*)obj);
		CEquipManager* eqmanager = (CEquipManager*)((char*)cship + 0xE4);
		CEquipTraverser tr(-1);
		CEquip *equip = eqmanager->Traverse(tr);
		while (equip)
		{
			if (CEGun::cast(equip))
			{
				CEGun* cgun = (CEGun*)equip;
				map<unsigned int, struct AmmoGenerator>::iterator iter = mapBurstFire.find(cgun->GunArch()->iArchID);
				if (iter != mapBurstFire.end())
				{
					ConPrint(L"BURSTFIRE: PlayerLaunch_AFTER - Iteration %d for client %u, removing 200 units of 0x%08X.\n", i, iClientID, iter->second.ammo);
					//HkRemoveCargo((const wchar_t*)Players.GetActiveCharacterName(iClientID), iter->second.ammo, 2000);	// better safe than sorry
					aClientData[iClientID].mapBurstFills[i].ammo = iter->second.ammo;
					aClientData[iClientID].mapBurstFills[i].count = iter->second.count;
					aClientData[iClientID].mapBurstFills[i].timestamp = 0;
					i++;
				}
			}
			equip = eqmanager->Traverse(tr);
		}
	}

	list<CARGO_INFO> lstCargo;
	int iRemainingHoldSize = 0;
	if (HkEnumCargo((const wchar_t*)Players.GetActiveCharacterName(iClientID), lstCargo, iRemainingHoldSize) == HKE_OK)
	{
		foreach(lstCargo, CARGO_INFO, item)
		{
			for (map<unsigned int, struct AmmoGenerator>::iterator iter = mapBurstFire.begin(); iter != mapBurstFire.end(); ++iter)
				if (item->iArchID == iter->second.ammo)
					HkRemoveCargo((const wchar_t*)Players.GetActiveCharacterName(iClientID), item->iID, item->iCount);
		}
	}
}

void HkTimerCheckKick()
{
	returncode = DEFAULT_RETURNCODE;

	uint curr_time = (uint)time(0);
	/*static int messageno = 1;

	wchar_t message[1024];
	swprintf_s(message, L"Hi! I'm an FLHook plugin! This is message number %d.", messageno++);*/
	//WriteKillposterSocket(message);
}

void HkTimerNPCAndF1Check()
{
	returncode = DEFAULT_RETURNCODE;

	if (!iLoadedBurstFires)
		return;

	mstime curr_mstime = GetTimeInMS();
	//ConPrint(L"BURSTFIRE: Tick %llu.\n", curr_mstime);
	for (int i = 0; i < 250; i++)
	{
		if (aClientData[i].mapBurstFills.size())
		{
			map<uint, uint> ammogenerator;
			//ConPrint(L"BURSTFIRE: Tick - Doing work for client %u.\n", i);
			for (map<ushort, struct AmmoGenerator>::iterator iter = aClientData[i].mapBurstFills.begin(); iter != aClientData[i].mapBurstFills.end(); ++iter)
			{
				if (curr_mstime >= iter->second.timestamp)
				{
					ammogenerator[iter->second.ammo] += iter->second.count;
					aClientData[i].mapBurstFills.erase(iter->first);
				}
			}

			for (map<uint, uint>::iterator iter = ammogenerator.begin(); iter != ammogenerator.end(); ++iter)
			{
				HkAddCargo((const wchar_t*)Players.GetActiveCharacterName(i), iter->first, iter->second, false);
				ConPrint(L"BURSTFIRE: Tick - Gave client %u %u rounds of 0x%08X.\n", i, iter->second, iter->first);
			}

			list<CARGO_INFO> lstCargo;
			int iRemainingHoldSize = 0;
			if (HkEnumCargo((const wchar_t*)Players.GetActiveCharacterName(i), lstCargo, iRemainingHoldSize) == HKE_OK)
			{
				foreach(lstCargo, CARGO_INFO, item)
				{
					for (map<unsigned int, struct AmmoGenerator>::iterator iter = mapBurstFire.begin(); iter != mapBurstFire.end(); ++iter)
						if (item->iArchID == iter->second.ammo)
							HkRemoveCargo((const wchar_t*)Players.GetActiveCharacterName(i), item->iID, item->iCount);
				}
			}
		}
	}
}

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
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ClearClientInfo, PLUGIN_ClearClientInfo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterSelect, PLUGIN_HkIServerImpl_CharacterSelect, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkCb_AddDmgEntry, PLUGIN_HkCb_AddDmgEntry, 9));
	//	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ShipDestroyed, PLUGIN_ShipDestroyed, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ExecuteCommandString_Callback, PLUGIN_ExecuteCommandString_Callback, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ReqModifyItem, PLUGIN_HkIServerImpl_ReqModifyItem, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&FireWeapon, PLUGIN_HkIServerImpl_FireWeapon, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SPScanCargo, PLUGIN_HkIServerImpl_SPScanCargo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch_AFTER, PLUGIN_HkIServerImpl_PlayerLaunch_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkTimerCheckKick, PLUGIN_HkTimerCheckKick, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkTimerNPCAndF1Check, PLUGIN_HkTimerNPCAndF1Check, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Plugin_Communication_Callback, PLUGIN_Plugin_Communication, 10));
	return p_PI;
}
