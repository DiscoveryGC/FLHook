/**
 Cloak (Yet another) Docking Plugin for FLHook-Plugin
 by Cannon.

0.1:
 Initial release
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
#include "Cloak.h"
#include <set>
#include <unordered_map>
#include <unordered_set>

static int set_iPluginDebug = 0;

/// A return code to indicate to FLHook if we want the hook processing to continue.
PLUGIN_RETURNCODE returncode;

enum INFO_STATE
{
	STATE_CLOAK_INVALID = 0,
	STATE_CLOAK_OFF = 1,
	STATE_CLOAK_CHARGING = 2,
	STATE_CLOAK_ON = 3,
};

struct CLOAK_ARCH
{
	string scNickName;
	int iWarmupTime;
	int iCooldownTime;
	int iHoldSizeLimit;
	map<uint, uint> mapFuelToUsage;
	bool singleUseCloak = false;
	bool bDropShieldsOnUncloak;
};

struct CLOAK_INFO
{
	CLOAK_INFO()
	{

		uint iCloakSlot = 0;
		bCanCloak = false;
		mstime tmCloakTime = 0;
		uint iState = STATE_CLOAK_INVALID;
		uint bAdmin = false;

		arch.iWarmupTime = 0;
		arch.iCooldownTime = 0;
		arch.iHoldSizeLimit = 0;
		arch.mapFuelToUsage.clear();
		arch.bDropShieldsOnUncloak = false;
	}

	uint iCloakSlot;
	bool bCanCloak;
	mstime tmCloakTime;
	uint iState;
	bool bAdmin;
	int DisruptTime;
	bool singleCloakConsumed = false;

	CLOAK_ARCH arch;
};

struct CDSTRUCT
{
	string nickname;
	float range;
	int cooldown;
	uint ammotype;
	uint ammoamount;
	uint effect;
	float effectlifetime;
};

struct CLIENTCDSTRUCT
{
	int cdwn;
	CDSTRUCT cd;
};

static unordered_map<uint, CLOAK_INFO> mapClientsCloak;
static map<uint, CLIENTCDSTRUCT> mapClientsCD;

static map<uint, CLOAK_ARCH> mapCloakingDevices;
static map<uint, CDSTRUCT> mapCloakDisruptors;

static unordered_set<uint> setJumpingClients;

void LoadSettings();

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	srand((uint)time(0));
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


void LoadSettings()
{
	// The path to the configuration file.
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\cloak.cfg";

	int cloakamt = 0;
	int cdamt = 0;
	int limitedamt = 0;
	INI_Reader ini;
	if (ini.open(scPluginCfgFile.c_str(), false))
	{
		while (ini.read_header())
		{
			CLOAK_ARCH device;
			CDSTRUCT disruptor;
			if (ini.is_header("Cloak"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("nickname"))
					{
						device.scNickName = ini.get_value_string(0);
					}
					else if (ini.is_value("warmup_time"))
					{
						device.iWarmupTime = ini.get_value_int(0);
					}
					else if (ini.is_value("cooldown_time"))
					{
						device.iCooldownTime = ini.get_value_int(0);
					}
					else if (ini.is_value("hold_size_limit"))
					{
						device.iHoldSizeLimit = ini.get_value_int(0);
					}
					else if (ini.is_value("fuel"))
					{
						string scNickName = ini.get_value_string(0);
						uint usage = ini.get_value_int(1);
						bool limitedCloak = ini.get_value_bool(2);
						if (limitedCloak) {
							limitedamt++;
						}
						device.singleUseCloak = limitedCloak;
						device.mapFuelToUsage[CreateID(scNickName.c_str())] = usage;
					}
					else if (ini.is_value("drop_shields_on_uncloak"))
					{
						device.bDropShieldsOnUncloak = ini.get_value_bool(0);
					}
				}
				mapCloakingDevices[CreateID(device.scNickName.c_str())] = device;
				++cloakamt;
			}
			else if (ini.is_header("Disruptor"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("nickname"))
					{
						disruptor.nickname = ini.get_value_string();
					}
					else if (ini.is_value("cooldown_time"))
					{
						disruptor.cooldown = ini.get_value_int(0);
					}
					else if (ini.is_value("range"))
					{
						disruptor.range = ini.get_value_float(0);
					}
					else if (ini.is_value("ammo"))
					{
						disruptor.ammotype = CreateID(ini.get_value_string(0));
						disruptor.ammoamount = ini.get_value_int(1);
					}
					else if (ini.is_value("fx"))
					{
						disruptor.effect = CreateID(ini.get_value_string());
					}
					else if (ini.is_value("effectlifetime"))
					{
						disruptor.effectlifetime = ini.get_value_float(0);
					}
				}
				mapCloakDisruptors[CreateID(disruptor.nickname.c_str())] = disruptor;
				++cdamt;
			}
		}
		ini.close();
	}

	ConPrint(L"CLOAK: Loaded %u cloaking devices \n", cloakamt);
	ConPrint(L"CLOAK: Loaded %u cloak disruptors \n", cdamt);
	ConPrint(L"CLOAK: Loaded %u limited cloak setups \n", limitedamt);

	struct PlayerData *pd = 0;
	while (pd = Players.traverse_active(pd))
	{
	}
}

void ClearClientInfo(uint iClientID)
{
	mapClientsCloak.erase(iClientID);
	setJumpingClients.erase(iClientID);
}

void SetCloak(uint iClientID, uint iShipID, bool bOn)
{
	XActivateEquip ActivateEq;
	ActivateEq.bActivate = bOn;
	ActivateEq.iSpaceID = iShipID;
	ActivateEq.sID = mapClientsCloak[iClientID].iCloakSlot;
	Server.ActivateEquip(iClientID, ActivateEq);
}

void SetState(uint iClientID, uint iShipID, int iNewState)
{
	if (mapClientsCloak[iClientID].iState != iNewState)
	{
		mapClientsCloak[iClientID].iState = iNewState;
		mapClientsCloak[iClientID].tmCloakTime = timeInMS();
		CLIENT_CLOAK_STRUCT communicationInfo;
		switch (iNewState)
		{
		case STATE_CLOAK_CHARGING:
		{
			communicationInfo.iClientID = iClientID;
			communicationInfo.isChargingCloak = true;
			communicationInfo.isCloaked = false;
			Plugin_Communication(CLIENT_CLOAK_INFO, &communicationInfo);

			PrintUserCmdText(iClientID, L"Preparing to cloak...");
			break;
		}

		case STATE_CLOAK_ON:
		{
			communicationInfo.iClientID = iClientID;
			communicationInfo.isChargingCloak = false;
			communicationInfo.isCloaked = true;
			Plugin_Communication(CLIENT_CLOAK_INFO, &communicationInfo);

			PrintUserCmdText(iClientID, L" Cloaking device on");
			SetCloak(iClientID, iShipID, true);
			PrintUserCmdText(iClientID, L"Cloaking device on");
			ClientInfo[iClientID].bCloaked = true;
			break;
		}
		case STATE_CLOAK_OFF:
		default:
		{
			communicationInfo.iClientID = iClientID;
			communicationInfo.isChargingCloak = false;
			communicationInfo.isCloaked = false;
			Plugin_Communication(CLIENT_CLOAK_INFO, &communicationInfo);

			PrintUserCmdText(iClientID, L" Cloaking device off");
			SetCloak(iClientID, iShipID, false);
			PrintUserCmdText(iClientID, L"Cloaking device off");
			ClientInfo[iClientID].bCloaked = false;
			break;
		}
		}
	}
}

bool removeSingleFuel(uint iClientID, CLOAK_INFO &info)
{
	for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
	{
		uint fuel_usage = info.arch.mapFuelToUsage[item->iArchID];
		if (item->iCount >= fuel_usage)
		{
			pub::Player::RemoveCargo(iClientID, item->sID, fuel_usage);
			info.singleCloakConsumed = true;
			return true;
		}
	}

	return false;

}

// Returns false if the ship has no fuel to operate its cloaking device.
static bool ProcessFuel(uint iClientID, CLOAK_INFO &info)
{
	if (info.bAdmin)
		return true;

	if (!info.singleCloakConsumed && info.arch.singleUseCloak) {
		return removeSingleFuel(iClientID, info);
	}

	else if (info.singleCloakConsumed)
	{
		return true;
	}

	for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
	{
		if (info.arch.mapFuelToUsage.find(item->iArchID) != info.arch.mapFuelToUsage.end())
		{
			uint fuel_usage = info.arch.mapFuelToUsage[item->iArchID];
			if (item->iCount >= fuel_usage)
			{
				pub::Player::RemoveCargo(iClientID, item->sID, fuel_usage);
				return true;
			}
		}
	}

	return false;
}

void PlayerLaunch_AFTER(unsigned int iShip, unsigned int iClientID)
{
	mapClientsCloak[iClientID].bCanCloak = false;
	mapClientsCloak[iClientID].bAdmin = false;


	for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
	{
		if (mapCloakDisruptors.find(item->iArchID) != mapCloakDisruptors.end())
		{
			if (item->bMounted)
			{
				//PrintUserCmdText(iClientID, L"DEBUG: Found relevant item: %s", stows(mapCloakDisruptors[item->equip.iArchID].nickname).c_str());
				pub::Audio::PlaySoundEffect(iClientID, CreateID("cargo_jettison"));

				CDSTRUCT cd;
				CLIENTCDSTRUCT cdclient;

				cdclient.cdwn = 0;
				cdclient.cd = mapCloakDisruptors[item->iArchID];

				mapClientsCD[iClientID] = cdclient;
				//PrintUserCmdText(iClientID, L"DEBUG: Checking data is correct: CD name is %s", stows(mapClientsCD[iClientID].cd.nickname).c_str());
			}
		}
	}

	//Legacy code for the cloaks, needs to be rewritten at some point. - Alley

	IObjInspectImpl *obj = HkGetInspect(iClientID);
	if (obj)
	{
		CShip* cship = (CShip*)HkGetEqObjFromObjRW((IObjRW*)obj);

		CEquipManager* eqmanager = (CEquipManager*)((char*)cship + 0xE4);
		CEquipTraverser tr(-1);
		CEquip *equip = eqmanager->Traverse(tr);

		while (equip)
		{
			if (CECloakingDevice::cast(equip))
			{
				mapClientsCloak[iClientID].iCloakSlot = equip->GetID();

				if (mapCloakingDevices.find(equip->EquipArch()->iArchID) != mapCloakingDevices.end())
				{
					// Otherwise set the fuel usage and warm up time
					mapClientsCloak[iClientID].arch = mapCloakingDevices[equip->EquipArch()->iArchID];
				}
				// If this cloaking device does not appear in the cloaking device list
				// then warming up and fuel usage is zero and it may be used by any
				// ship.
				else
				{
					mapClientsCloak[iClientID].arch.bDropShieldsOnUncloak = false;
					mapClientsCloak[iClientID].arch.iCooldownTime = 0;
					mapClientsCloak[iClientID].arch.iHoldSizeLimit = 0;
					mapClientsCloak[iClientID].arch.iWarmupTime = 0;
					mapClientsCloak[iClientID].arch.mapFuelToUsage.clear();
				}

				mapClientsCloak[iClientID].DisruptTime = 0;
				mapClientsCloak[iClientID].bCanCloak = true;
				mapClientsCloak[iClientID].iState = STATE_CLOAK_INVALID;
				SetState(iClientID, iShip, STATE_CLOAK_OFF);
				return;
			}

			equip = eqmanager->Traverse(tr);

		}
	}
}

void BaseEnter(unsigned int iBaseID, unsigned int iClientID)
{
	mapClientsCloak.erase(iClientID);
	mapClientsCD.erase(iClientID);
}

void HkTimerCheckKick()
{
	mstime now = timeInMS();
	uint curr_time = (uint)time(0);


	for (auto& ci = mapClientsCloak.begin(); ci != mapClientsCloak.end(); ++ci)
	{
		uint iClientID = ci->first;
		uint iShipID = Players[iClientID].iShipID;
		CLOAK_INFO &info = ci->second;

		//code to check if the player is disrupted. We run this separately to not cause issues with the bugfix
		//first we check if it's 0. If it is, it's useless to process the other conditions, even if it means an additional check to begin with.
		if (info.DisruptTime != 0)
		{
			if (info.DisruptTime >= 2)
			{
				--info.DisruptTime;
			}
			else if (info.DisruptTime == 1)
			{
				PrintUserCmdText(iClientID, L"Cloaking Device restored.");
				info.DisruptTime = 0;
			}
		}

		if (iShipID && info.bCanCloak)
		{
			switch (info.iState)
			{
			case STATE_CLOAK_OFF:
				// Send cloak state for uncloaked cloak-able players (only for them in space)
				// this is the code to fix the bug where players wouldnt always see uncloaked players
				XActivateEquip ActivateEq;
				ActivateEq.bActivate = false;
				ActivateEq.iSpaceID = iShipID;
				ActivateEq.sID = info.iCloakSlot;
				Server.ActivateEquip(iClientID, ActivateEq);
				break;

			case STATE_CLOAK_CHARGING:
				if (!ProcessFuel(iClientID, info))
				{
					PrintUserCmdText(iClientID, L"Cloaking device shutdown, no fuel");
					SetState(iClientID, iShipID, STATE_CLOAK_OFF);
				}
				else if (setJumpingClients.find(iClientID) != setJumpingClients.end())
				{
					PrintUserCmdText(iClientID, L"Cloaking device shutdown, jumping");
					SetState(iClientID, iShipID, STATE_CLOAK_OFF);

					wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
					HkAddCheaterLog(wscCharname, L"Attempting to cloak charge while on list of clients currently jumping");
				}
				else if ((info.tmCloakTime + info.arch.iWarmupTime) < now)
				{
					SetState(iClientID, iShipID, STATE_CLOAK_ON);
				}
				else if (info.arch.bDropShieldsOnUncloak && !info.bAdmin)
				{
					pub::SpaceObj::DrainShields(iShipID);
				}
				break;

			case STATE_CLOAK_ON:
				if (!ProcessFuel(iClientID, info))
				{
					PrintUserCmdText(iClientID, L"Cloaking device shutdown, no fuel");
					SetState(iClientID, iShipID, STATE_CLOAK_OFF);
				}
				else if (info.arch.bDropShieldsOnUncloak && !info.bAdmin)
				{
					pub::SpaceObj::DrainShields(iShipID);


					if ((curr_time % 5) == 0)
					{
						uint iShip;
						pub::Player::GetShip(iClientID, iShip);

						Vector pos;
						Matrix rot;
						pub::SpaceObj::GetLocation(iShipID, pos, rot);

						uint iSystem;
						pub::Player::GetSystem(iClientID, iSystem);

						uint MusictoID = CreateID("dsy_jumpdrive_survey");
						//pub::Audio::PlaySoundEffect(iClientID, MusictoID);

						// For all players in system...
						struct PlayerData *pPD = 0;
						while (pPD = Players.traverse_active(pPD))
						{
							// Get the this player's current system and location in the system.
							uint client2 = HkGetClientIdFromPD(pPD);
							uint iSystem2 = 0;
							pub::Player::GetSystem(client2, iSystem2);
							if (iSystem != iSystem2)
								continue;

							uint iShip2;
							pub::Player::GetShip(client2, iShip2);

							Vector pos2;
							Matrix rot2;
							pub::SpaceObj::GetLocation(iShip2, pos2, rot2);

							// Is player within the specified range of the sending char.
							if (HkDistance3D(pos, pos2) > 4000)
								continue;

							pub::Audio::PlaySoundEffect(client2, MusictoID);
						}
					}
				}
				break;
			}
		}
	}
	for (map<uint, CLIENTCDSTRUCT>::iterator cd = mapClientsCD.begin(); cd != mapClientsCD.end(); ++cd)
	{
		if (cd->second.cdwn >= 2)
		{
			--cd->second.cdwn;
		}
		else if (cd->second.cdwn == 1)
		{
			PrintUserCmdText(cd->first, L"Cloak Disruptor cooldown complete.");
			IObjInspectImpl *obj = HkGetInspect(cd->first);
			if (obj)
			{
				HkUnLightFuse((IObjRW*)obj, cd->second.cd.effect, 0.0f);
			}
			cd->second.cdwn = 0;
		}
	}
}

void CloakDisruptor(uint iClientID)
{
	uint iShipID = Players[iClientID].iShipID;

	Vector pos;
	Matrix rot;
	pub::SpaceObj::GetLocation(iShipID, pos, rot);

	uint iSystem;
	pub::Player::GetSystem(iClientID, iSystem);

	// For all players in system...
	struct PlayerData *pPD = 0;
	while (pPD = Players.traverse_active(pPD))
	{
		// Get the this player's current system and location in the system.
		uint client2 = HkGetClientIdFromPD(pPD);
		uint iSystem2 = 0;
		pub::Player::GetSystem(client2, iSystem2);
		if (iSystem != iSystem2)
			continue;

		uint iShip2;
		pub::Player::GetShip(client2, iShip2);

		Vector pos2;
		Matrix rot2;
		pub::SpaceObj::GetLocation(iShip2, pos2, rot2);

		// Is player within the specified range of the sending char.
		if (HkDistance3D(pos, pos2) > mapClientsCD[iClientID].cd.range)
			continue;

		//we check if that character has a cloaking device.
		if (mapClientsCloak[client2].bCanCloak == true)
		{
			//if it's an admin, do nothing. Doing it this way fixes the ghost bug.
			if (mapClientsCloak[client2].bAdmin == true)
			{

			}
			else
			{
				//check if the cloak is charging or is already on
				if (mapClientsCloak[client2].iState == STATE_CLOAK_CHARGING || mapClientsCloak[client2].iState == STATE_CLOAK_ON)
				{
					SetState(client2, iDmgToSpaceID, STATE_CLOAK_OFF);
					pub::Audio::PlaySoundEffect(client2, CreateID("cloak_osiris"));
					PrintUserCmdText(client2, L"Alert: Cloaking device disruption field detected.");
					mapClientsCloak[client2].DisruptTime = mapClientsCD[iClientID].cd.cooldown;
				}
			}
		}
	}
}

void SetFuse(uint iClientID, uint fuse, float lifetime)
{
	CDSTRUCT &cd = mapClientsCD[iClientID].cd;

	IObjInspectImpl *obj = HkGetInspect(iClientID);
	if (obj)
	{
		HkLightFuse((IObjRW*)obj, fuse, 0.0f, lifetime, 0.0f);
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UserCmd_Cloak(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	uint iShip;
	pub::Player::GetShip(iClientID, iShip);
	if (!iShip)
	{
		PrintUserCmdText(iClientID, L"Not in space");
		return true;
	}

	if (!mapClientsCloak[iClientID].bCanCloak)
	{
		PrintUserCmdText(iClientID, L"Cloaking device not available");
		return true;
	}

	if (mapClientsCloak[iClientID].DisruptTime != 0)
	{
		PrintUserCmdText(iClientID, L"Cloaking Device Disrupted. Please wait %d seconds", mapClientsCloak[iClientID].DisruptTime);
		return true;
	}

	// If this cloaking device requires more power than the ship can provide
	// no cloaking device is available.
	IObjInspectImpl *obj = HkGetInspect(iClientID);
	if (obj)
	{
		CShip* cship = (CShip*)HkGetEqObjFromObjRW((IObjRW*)obj);
		if (cship)
		{
			if (mapClientsCloak[iClientID].arch.iHoldSizeLimit != 0
				&& mapClientsCloak[iClientID].arch.iHoldSizeLimit < cship->shiparch()->fHoldSize)
			{
				PrintUserCmdText(iClientID, L"Cloaking device will not function on this ship type");
				mapClientsCloak[iClientID].iState = STATE_CLOAK_INVALID;
				SetState(iClientID, iShip, STATE_CLOAK_OFF);
				return true;
			}

			switch (mapClientsCloak[iClientID].iState)
			{
			case STATE_CLOAK_OFF:
				SetState(iClientID, iShip, STATE_CLOAK_CHARGING);
				break;
			case STATE_CLOAK_CHARGING:
			case STATE_CLOAK_ON:
				SetState(iClientID, iShip, STATE_CLOAK_OFF);
				break;
			}
		}
	}
	return true;
}

bool UserCmd_Disruptor(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	//not found
	if (mapClientsCD.find(iClientID) == mapClientsCD.end())
	{
		PrintUserCmdText(iClientID, L"Cloak Disruptor not found.");
	}
	//found
	else
	{
		if (mapClientsCD[iClientID].cdwn != 0)
		{
			PrintUserCmdText(iClientID, L"Cloak Disruptor recharging. %d seconds left.", mapClientsCD[iClientID].cdwn);
		}
		else
		{
			//bugfix, initial status assumes every player don't have ammo.
			bool foundammo = false;

			for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
			{
				if (item->iArchID == mapClientsCD[iClientID].cd.ammotype)
				{
					if (item->iCount >= mapClientsCD[iClientID].cd.ammoamount)
					{
						pub::Player::RemoveCargo(iClientID, item->sID, mapClientsCD[iClientID].cd.ammoamount);
						foundammo = true;
						break;
					}
				}
			}

			if (foundammo == false)
			{
				PrintUserCmdText(iClientID, L"ERR Not enough batteries.");
				return true;
			}

			IObjInspectImpl *obj = HkGetInspect(iClientID);
			if (obj)
			{
				HkLightFuse((IObjRW*)obj, mapClientsCD[iClientID].cd.effect, 0, mapClientsCD[iClientID].cd.effectlifetime, 0);
			}

			pub::Audio::PlaySoundEffect(iClientID, CreateID("cloak_osiris"));
			CloakDisruptor(iClientID);
			mapClientsCD[iClientID].cdwn = mapClientsCD[iClientID].cd.cooldown;
			PrintUserCmdText(iClientID, L"Cloak Disruptor engaged. Cooldown: %d seconds.", mapClientsCD[iClientID].cdwn);
		}
	}

	return true;
}


typedef bool(*_UserCmdProc)(uint, const wstring &, const wstring &, const wchar_t*);

struct USERCMD
{
	wchar_t *wszCmd;
	_UserCmdProc proc;
	wchar_t *usage;
};

USERCMD UserCmds[] =
{
	{ L"/cloak", UserCmd_Cloak, L"Usage: /cloak"},
	{ L"/cloak*", UserCmd_Cloak, L"Usage: /cloak"},
	{ L"/disruptor", UserCmd_Disruptor, L"Usage: /disruptor"},
	{ L"/disruptor*", UserCmd_Disruptor, L"Usage: /disruptor"},

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
				// We handled the command tell FL hook to stop processing this
				// chat string.
				returncode = SKIPPLUGINS_NOFUNCTIONCALL; // we handled the command, return immediatly
				return true;
			}
		}
	}
	return false;
}

#define IS_CMD(a) !wscCmd.compare(L##a)

bool ExecuteCommandString_Callback(CCmds* cmds, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	if (IS_CMD("cloak"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		uint iClientID = HkGetClientIdFromCharname(cmds->GetAdminName());
		
		if (!(cmds->rights & RIGHT_CLOAK)) { cmds->Print(L"ERR No permission\n"); return true; }
		
		if (iClientID == -1)
		{
			cmds->Print(L"ERR On console");
			return true;
		}

		uint iShip;
		pub::Player::GetShip(iClientID, iShip);
		if (!iShip)
		{
			PrintUserCmdText(iClientID, L"ERR Not in space");
			return true;
		}

		if (!mapClientsCloak[iClientID].bCanCloak)
		{
			cmds->Print(L"ERR Cloaking device not available");
			return true;
		}

		switch (mapClientsCloak[iClientID].iState)
		{
		case STATE_CLOAK_OFF:
			mapClientsCloak[iClientID].bAdmin = true;
			SetState(iClientID, iShip, STATE_CLOAK_ON);
			break;
		case STATE_CLOAK_CHARGING:
		case STATE_CLOAK_ON:
			mapClientsCloak[iClientID].bAdmin = true;
			SetState(iClientID, iShip, STATE_CLOAK_OFF);
			break;
		}
		return true;
	}
	return false;
}

void __stdcall HkCb_AddDmgEntry(DamageList *dmg, unsigned short p1, float& damage, enum DamageEntry::SubObjFate& fate)
{
	returncode = DEFAULT_RETURNCODE;
	if (!iDmgToSpaceID || !dmg->get_inflictor_id())
		return;

	if (dmg->get_cause() != 0x06 && dmg->get_cause() != 0x15)
		return;
	
	uint client = HkGetClientIDByShip(iDmgToSpaceID);
	if (!client)
		return;

	if (mapClientsCloak[client].bCanCloak
		&& !mapClientsCloak[client].bAdmin
		&& mapClientsCloak[client].iState == STATE_CLOAK_CHARGING)
	{
		SetState(client, iDmgToSpaceID, STATE_CLOAK_OFF);
	}
}

void __stdcall JumpInComplete_AFTER(unsigned int iSystem, unsigned int iShip)
{
	returncode = DEFAULT_RETURNCODE;
	uint iClientID = HkGetClientIDByShip(iShip);
	setJumpingClients.erase(iClientID);

	if (iClientID && mapClientsCloak[iClientID].iState == STATE_CLOAK_CHARGING)
	{


		SetState(iClientID, iShip, STATE_CLOAK_OFF);
		pub::Audio::PlaySoundEffect(iClientID, CreateID("cloak_osiris"));
		PrintUserCmdText(iClientID, L"Alert: Cloaking device overheat detected. Shutting down.");

		wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
		HkAddCheaterLog(wscCharname, L"Switched system while under cloak charging mode");
	}
}

int __cdecl Dock_Call(unsigned int const &iShip, unsigned int const &iDockTarget, int iCancel, enum DOCK_HOST_RESPONSE response)
{
	returncode = DEFAULT_RETURNCODE;

	uint client = HkGetClientIDByShip(iShip);
	if (client)
	{
		if ((response == PROCEED_DOCK || response == DOCK) && iCancel == -1)
		{
			uint iTypeID;
			pub::SpaceObj::GetType(iDockTarget, iTypeID);
			if (iTypeID == OBJ_JUMP_GATE || iTypeID == OBJ_JUMP_HOLE)
			{
				setJumpingClients.erase(client);
			}
		}
		if ((response == PROCEED_DOCK || response == DOCK) && iCancel != -1)
		{
			// If the last jump happened within 60 seconds then prohibit the docking
			// on a jump hole or gate.
			uint iTypeID;
			pub::SpaceObj::GetType(iDockTarget, iTypeID);
			if (iTypeID == OBJ_JUMP_GATE || iTypeID == OBJ_JUMP_HOLE)
			{
				setJumpingClients.insert(client);
				if (client && mapClientsCloak[client].iState == STATE_CLOAK_CHARGING)
				{
					SetState(client, iShip, STATE_CLOAK_OFF);
					pub::Audio::PlaySoundEffect(client, CreateID("cloak_osiris"));
					PrintUserCmdText(client, L"Alert: Cloaking device overheat detected. Shutting down.");

					wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(client);
					HkAddCheaterLog(wscCharname, L"About to enter a JG/JH while under cloak charging mode");
					return 0;
				}
			}
		}
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Functions to hook */
EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "Cloak Plugin by cannon";
	p_PI->sShortName = "cloak";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ClearClientInfo, PLUGIN_ClearClientInfo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch_AFTER, PLUGIN_HkIServerImpl_PlayerLaunch_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseEnter, PLUGIN_HkIServerImpl_BaseEnter, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkTimerCheckKick, PLUGIN_HkTimerCheckKick, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ExecuteCommandString_Callback, PLUGIN_ExecuteCommandString_Callback, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkCb_AddDmgEntry, PLUGIN_HkCb_AddDmgEntry, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&JumpInComplete_AFTER, PLUGIN_HkIServerImpl_JumpInComplete_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Dock_Call, PLUGIN_HkCb_Dock_Call, 0));


	return p_PI;
}
