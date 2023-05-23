// Player Control plugin for FLHookPlugin
// Feb 2010 by Cannon
//
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

// includes 
#include <windows.h>
#include <stdio.h>
#include <boost/algorithm/string/case_conv.hpp>
#include <string>
#include <time.h>
#include <math.h>
#include <float.h>
#include <FLHook.h>
#include <plugin.h>
#include <math.h>
#include <list>
#include <set>


#include <PluginUtilities.h>
#include "ZoneUtilities.h"
#include "CrashCatcher.h"
#include "Main.h"
#include "StartupCache.h"
#include "wildcards.h"

// Current configuration
int set_iPluginDebug = 0;

//Arbitrary name of the Hera Torpedo ammo, for JD disruption
//uint JDDisruptAmmo = 0;

/// True if loot logging is enabled
bool set_bLogLooting = false;

// Disable various user features
bool set_bEnablePimpShip = false;
bool set_bEnableRenameMe = false;
bool set_bEnableMoveChar = false;
bool set_bEnableRestart = false;
bool set_bEnableGiveCash = false;
bool set_bLocalTime = false;

/// Local chat range
float set_iLocalChatRange = 9999;
float set_iDockBroadcastRange = 9999;

float set_fSpinProtectMass;
float set_fSpinImpulseMultiplier;

/** A return code to indicate to FLHook if we want the hook processing to continue. */
PLUGIN_RETURNCODE returncode;

// TODO: detect frequent /stuck use and blow up ship (or something).

///////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LoadSettings();

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
		HkLoadStringDLLs();
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
		CrashCatcher::Shutdown();
		HkUnloadStringDLLs();
	}
	return true;
}


/// Hook will call this function after calling a plugin function to see if we the
/// processing to continue
EXPORT PLUGIN_RETURNCODE Get_PluginReturnCode()
{
	return returncode;
}

/// Load the configuration
void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	// The path to the configuration file.
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\playercntl.cfg";

	set_iPluginDebug = IniGetI(scPluginCfgFile, "General", "Debug", 0);
	if (set_iPluginDebug)
		ConPrint(L"NOTICE: player control debug=%d\n", set_iPluginDebug);

	set_bLogLooting = IniGetB(scPluginCfgFile, "General", "LogLooting", false);
	set_bEnableMoveChar = IniGetB(scPluginCfgFile, "General", "EnableMoveChar", false);
	set_bEnableRenameMe = IniGetB(scPluginCfgFile, "General", "EnableRenameMe", false);
	set_bEnablePimpShip = IniGetB(scPluginCfgFile, "General", "EnablePimpShip", false);
	set_bEnableRestart = IniGetB(scPluginCfgFile, "General", "EnableRestart", false);
	set_bEnableGiveCash = IniGetB(scPluginCfgFile, "General", "EnableGiveCash", false);

	set_fSpinProtectMass = IniGetF(scPluginCfgFile, "General", "SpinProtectionMass", 180.0f);
	set_fSpinImpulseMultiplier = IniGetF(scPluginCfgFile, "General", "SpinProtectionMultiplier", -1.0f);

	set_iLocalChatRange = IniGetF(scPluginCfgFile, "General", "LocalChatRange", 0);
	set_iDockBroadcastRange = IniGetF(scPluginCfgFile, "General", "DockBroadcastRange", 0);

	set_bLocalTime = IniGetB(scPluginCfgFile, "General", "LocalTime", false);

	//JDDisruptAmmo = CreateID("dsy_torpedo_jd_ammo");

	ZoneUtilities::ReadUniverse();
	EquipmentUtilities::ReadIniNicknames();
	Rename::LoadSettings(scPluginCfgFile);
	GiveCash::LoadSettings(scPluginCfgFile);
	MiscCmds::LoadSettings(scPluginCfgFile);
	PimpShip::LoadSettings(scPluginCfgFile);
	HyperJump::LoadSettings(scPluginCfgFile);
	PurchaseRestrictions::LoadSettings(scPluginCfgFile);
	IPBans::LoadSettings(scPluginCfgFile);
	CargoDrop::LoadSettings(scPluginCfgFile);
	Restart::LoadSettings(scPluginCfgFile);
	RepFixer::LoadSettings(scPluginCfgFile);
	Message::LoadSettings(scPluginCfgFile);
	SystemSensor::LoadSettings(scPluginCfgFile);
	CrashCatcher::Init();
	Rename::ReloadLockedShips();
}

/** Clean up when a client disconnects */
void ClearClientInfo(uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	MiscCmds::ClearClientInfo(iClientID);
	HyperJump::ClearClientInfo(iClientID);
	CargoDrop::ClearClientInfo(iClientID);
	IPBans::ClearClientInfo(iClientID);
	Message::ClearClientInfo(iClientID);
	PurchaseRestrictions::ClearClientInfo(iClientID);
	AntiJumpDisconnect::ClearClientInfo(iClientID);
	SystemSensor::ClearClientInfo(iClientID);
}

/** One second timer */
void HkTimer()
{
	returncode = DEFAULT_RETURNCODE;
	MiscCmds::Timer();
	HyperJump::Timer();
	CargoDrop::Timer();
	Message::Timer();
	Restart::Timer();
	Rename::Timer();
}

/// Hook for ship distruction. It's easier to hook this than the PlayerDeath one.
/// Drop a percentage of cargo + some loot representing ship bits.
void SendDeathMsg(const wstring &wscMsg, uint iSystem, uint iClientIDVictim, uint iClientIDKiller)
{
	returncode = NOFUNCTIONCALL;

	HyperJump::SendDeathMsg(wscMsg, iSystem, iClientIDVictim, iClientIDKiller);
	CargoDrop::SendDeathMsg(wscMsg, iSystem, iClientIDVictim, iClientIDKiller);
	Message::SendDeathMsg(wscMsg, iSystem, iClientIDVictim, iClientIDKiller);

	const wchar_t *victim = (const wchar_t*)Players.GetActiveCharacterName(iClientIDVictim);
	const wchar_t *killer = (const wchar_t*)Players.GetActiveCharacterName(iClientIDKiller);
	if (victim && killer)
	{
		AddLog("NOTICE: Death charname=%s killername=%s system=%08x",
			wstos(victim).c_str(), wstos(killer).c_str(), iSystem);
	}
}

/*
void __stdcall SPMunitionCollision(struct SSPMunitionCollisionInfo const & ci, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	// If this is not a ship, do no other processing.
	if (ci.dwTargetShip == 0)
		return;

	if (JDDisruptAmmo == ci.iProjectileArchID)
	{
		uint ship = ci.dwTargetShip;
		uint targetid = HkGetClientIDByShip(ship);

		if (targetid)
		{
			HyperJump::Disrupt(targetid, iClientID);
		}
		else
		{
			HkMsgU(L"Debug: targetid invalid HyperJump::Disrupt");
		}
	}

	returncode = DEFAULT_RETURNCODE;
	return;
}
*/


static bool IsDockingAllowed(uint iShip, uint iDockTarget, uint iClientID)
{
	// If the player's rep is less/equal -0.55 to the owner of the station
	// then refuse the docking request
	int iSolarRep;
	pub::SpaceObj::GetSolarRep(iDockTarget, iSolarRep);

	int iPlayerRep;
	pub::SpaceObj::GetRep(iShip, iPlayerRep);

	float fAttitude = 0.0f;
	pub::Reputation::GetAttitude(iSolarRep, iPlayerRep, fAttitude);

	CUSTOM_BASE_IS_IT_POB_STRUCT info;
	info.iBase = iDockTarget;
	info.bAnswer = false;
	Plugin_Communication(CUSTOM_IS_IT_POB, &info);
	if (info.bAnswer)
		return true;

	if (fAttitude <= -0.55f)
	{
		pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("info_access_denied"));
		wstring wscMsg[3] = {
			L"Access Denied! Request to dock denied. We don't want your kind around here.",
			L"Access Denied! Docking request rejected. Your papers are no good.",
			L"Access Denied! You can't dock here. Your reputation stinks."
		};
		PrintUserCmdText(iClientID, wscMsg[rand() % 3]);
		return false;
	}

	return true;
}

// Determine the path name of a file in the charname account directory with the
// provided extension. The resulting path is returned in the path parameter.
bool GetUserFilePath(string &path, const wstring &wscCharname, const string &extension)
{
	// init variables
	char szDataPath[MAX_PATH];
	GetUserDataPath(szDataPath);
	string scAcctPath = string(szDataPath) + "\\Accts\\MultiPlayer\\";

	wstring wscDir;
	wstring wscFile;
	if (HkGetAccountDirName(wscCharname, wscDir) != HKE_OK)
		return false;
	if (HkGetCharFileName(wscCharname, wscFile) != HKE_OK)
		return false;
	path = scAcctPath + wstos(wscDir) + "\\" + wstos(wscFile) + extension;
	return true;
}

string GetUserFilePath(const wstring &wscCharname, const string &scExtension)
{
	// init variables
	char szDataPath[MAX_PATH];
	GetUserDataPath(szDataPath);
	string scAcctPath = string(szDataPath) + "\\Accts\\MultiPlayer\\";

	wstring wscDir;
	wstring wscFile;
	if (HkGetAccountDirName(wscCharname, wscDir) != HKE_OK)
		return "";
	if (HkGetCharFileName(wscCharname, wscFile) != HKE_OK)
		return "";

	return scAcctPath + wstos(wscDir) + "\\" + wstos(wscFile) + scExtension;
}

namespace HkIEngine
{
	int __cdecl Dock_Call(unsigned int const &iShip, unsigned int const &iDockTarget, int iCancel, enum DOCK_HOST_RESPONSE response)
	{
		returncode = DEFAULT_RETURNCODE;

		uint iClientID = HkGetClientIDByShip(iShip);

		if (iClientID == 0)
		{
			// NPC call, let the game handle it
			return 0;
		}
		else
		{
			if (Players[iClientID].fRelativeHealth == 0.0f && (iCancel != -1)) {
				pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("dock_disallowed"));
				returncode = SKIPPLUGINS_NOFUNCTIONCALL;
				return 0;
			}
			uint iTypeID;
			pub::SpaceObj::GetType(iDockTarget, iTypeID);
			if (iTypeID == OBJ_DOCKING_RING || iTypeID == OBJ_STATION)
			{
				if (!IsDockingAllowed(iShip, iDockTarget, iClientID))
				{
					//AddLog("INFO: Docking suppressed docktarget=%u charname=%s", iDockTarget, wstos(Players.GetActiveCharacterName(iClientID)).c_str());
					returncode = SKIPPLUGINS_NOFUNCTIONCALL;
					return 0;
				}

				if (response == PROCEED_DOCK)
				{
					// Print out a message when a player ship docks.
					wstring wscMsg = L"%time Traffic control alert: %player has requested to dock";
					wscMsg = ReplaceStr(wscMsg, L"%time", GetTimeString(set_bLocalTime));
					wscMsg = ReplaceStr(wscMsg, L"%player", (const wchar_t*)Players.GetActiveCharacterName(iClientID));
					PrintLocalUserCmdText(iClientID, wscMsg, set_iDockBroadcastRange);
				}
			}

			SystemSensor::Dock_Call(iShip, iDockTarget, iCancel, response);
			return 0;
		}
	}
}

namespace HkIServerImpl
{
	bool __stdcall Startup_AFTER(struct SStartupInfo const &p1)
	{
		returncode = DEFAULT_RETURNCODE;
		StartupCache::Done();
		return true;
	}

	bool __stdcall Startup(struct SStartupInfo const &p1)
	{
		returncode = DEFAULT_RETURNCODE;
		StartupCache::Init();
		return true;
	}


	// The startup cache disables reading of the banned file. Check this manually on
	// login and boot the player if they are banned.
	void __stdcall Login(struct SLoginInfo const &li, unsigned int iClientID)
	{
		returncode = DEFAULT_RETURNCODE;

		CAccount *acc = Players.FindAccountFromClientID(iClientID);
		if (acc)
		{
			wstring wscDir;
			HkGetAccountDirName(acc, wscDir);

			char szDataPath[MAX_PATH];
			GetUserDataPath(szDataPath);

			string path = string(szDataPath) + "\\Accts\\MultiPlayer\\" + wstos(wscDir) + "\\banned";

			FILE *file = fopen(path.c_str(), "r");
			if (file)
			{
				fclose(file);

				// Ban the player
				flstr *flStr = CreateWString(acc->wszAccID);
				Players.BanAccount(*flStr, true);
				FreeWString(flStr);

				// Kick them
				acc->ForceLogout();

				// And stop further processing.
				returncode = SKIPPLUGINS_NOFUNCTIONCALL;
			}
		}
	}

	void __stdcall CreateNewCharacter(struct SCreateCharacterInfo const &si, unsigned int iClientID)
	{
		returncode = DEFAULT_RETURNCODE;
		if (Rename::CreateNewCharacter(si, iClientID))
		{
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
			Server.CharacterInfoReq(iClientID, true);
		}
	}

	void __stdcall DestroyCharacter(struct CHARACTER_ID const &cId, unsigned int iClientID)
	{
		returncode = DEFAULT_RETURNCODE;
		if (Rename::DestroyCharacter(cId, iClientID))
		{
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
			Server.CharacterInfoReq(iClientID, true);
		}
	}

	void __stdcall CreateGuided(uint iClientID, FLPACKET_CREATEGUIDED& createGuidedPacket)
	{
		uint clientID = HkGetClientIDByShip(createGuidedPacket.iOwner);
		if (!clientID)
			return;
		uint targetId;
		pub::SpaceObj::GetTarget(createGuidedPacket.iOwner, targetId);
		if (targetId)
			return;

		const auto& projectile = reinterpret_cast<CGuided*>(CObject::Find(createGuidedPacket.iProjectileId, CObject::CGUIDED_OBJECT));
		projectile->set_target(nullptr);
	}

	void __stdcall RequestEvent(int iIsFormationRequest, unsigned int iShip, unsigned int iDockTarget, unsigned int p4, unsigned long p5, unsigned int iClientID)
	{
		returncode = DEFAULT_RETURNCODE;
		if (iClientID)
		{
			if (!iIsFormationRequest)
			{
				uint iTargetTypeID;
				pub::SpaceObj::GetType(iDockTarget, iTargetTypeID);
				if (iTargetTypeID == OBJ_DOCKING_RING || iTargetTypeID == OBJ_STATION)
				{
					if (!IsDockingAllowed(iShip, iDockTarget, iClientID))
					{
						//AddLog("INFO: Docking suppressed docktarget=%u charname=%s", iDockTarget, wstos(Players.GetActiveCharacterName(iClientID)).c_str());
						returncode = SKIPPLUGINS_NOFUNCTIONCALL;
						return;
					}
				}
			}
		}
	}

	void __stdcall PlayerLaunch(unsigned int iShip, unsigned int iClientID)
	{
		returncode = DEFAULT_RETURNCODE;

		// TODO if the player is kicked abort processing.
		IPBans::PlayerLaunch(iShip, iClientID);
		RepFixer::PlayerLaunch(iShip, iClientID);
		Message::PlayerLaunch(iShip, iClientID);
		GiveCash::PlayerLaunch(iShip, iClientID);
		PurchaseRestrictions::PlayerLaunch(iShip, iClientID);
		HyperJump::PlayerLaunch(iShip, iClientID);
		SystemSensor::PlayerLaunch(iShip, iClientID);
	}

	void __stdcall PlayerLaunch_AFTER(unsigned int iShip, unsigned int iClientID)
	{
		HyperJump::CheckForMatrix(iClientID);
		returncode = DEFAULT_RETURNCODE;
	}

	void __stdcall BaseEnter(unsigned int iBaseID, unsigned int iClientID)
	{
		returncode = DEFAULT_RETURNCODE;
		IPBans::BaseEnter(iBaseID, iClientID);
		RepFixer::BaseEnter(iBaseID, iClientID);
		Message::BaseEnter(iBaseID, iClientID);
		GiveCash::BaseEnter(iBaseID, iClientID);
		PurchaseRestrictions::BaseEnter(iBaseID, iClientID);
		MiscCmds::BaseEnter(iBaseID, iClientID);
	}

	void __stdcall BaseEnter_AFTER(unsigned int iBaseID, unsigned int iClientID)
	{
		float fValue;
		if (HKGetShipValue((const wchar_t*)Players.GetActiveCharacterName(iClientID), fValue) == HKE_OK)
		{
			if (fValue > 2100000000.0f)
			{
				AddLog("ERROR: Possible corrupt ship charname=%s asset_value=%0.0f", wstos((const wchar_t*)Players.GetActiveCharacterName(iClientID)).c_str(), fValue);
			}
		}
	}

	void __stdcall LocationEnter(unsigned int iLocationID, unsigned int iClientID)
	{
		returncode = DEFAULT_RETURNCODE;
		PimpShip::LocationEnter(iLocationID, iClientID);
	}

	void __stdcall DisConnect(unsigned int iClientID, enum  EFLConnection state)
	{
		returncode = DEFAULT_RETURNCODE;
		ClearClientInfo(iClientID);
	}

	void __stdcall CharacterSelect_AFTER(struct CHARACTER_ID const &charId, unsigned int iClientID)
	{
		returncode = DEFAULT_RETURNCODE;
		ClearClientInfo(iClientID);
		Rename::CharacterSelect_AFTER(charId, iClientID);
	}

	void __stdcall JumpInComplete_AFTER(unsigned int iSystem, unsigned int iShip)
	{
		returncode = DEFAULT_RETURNCODE;
		uint iClientID = HkGetClientIDByShip(iShip);
		if (iClientID)
		{
			AntiJumpDisconnect::JumpInComplete(iSystem, iShip, iClientID);
			SystemSensor::JumpInComplete(iSystem, iShip, iClientID);
		}

		// Make player damageable once the ship has jumped in system.
		pub::SpaceObj::SetInvincible(iShip, false, false, 0);
	}

	void __stdcall SystemSwitchOutComplete(unsigned int iShip, unsigned int iClientID)
	{
		returncode = DEFAULT_RETURNCODE;
		// Make player invincible to fix JHs/JGs near mine fields sometimes
		// exploding player while jumping (in jump tunnel)
		pub::SpaceObj::SetInvincible(iShip, true, true, 0);
		AntiJumpDisconnect::SystemSwitchOutComplete(iShip, iClientID);
		if (HyperJump::SystemSwitchOutComplete(iShip, iClientID))
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
	}

	void __stdcall SPObjCollision(struct SSPObjCollisionInfo const &ci, unsigned int iClientID)
	{
		returncode = DEFAULT_RETURNCODE;

		// If spin protection is off, do nothing.
		if (set_fSpinProtectMass == -1.0f)
			return;

		// If the target is not a player, do nothing.
		//uint iClientIDTarget = HkGetClientIDByShip(ci.dwTargetShip);
		//if (iClientIDTarget<=0)
		//	return;

		float target_mass;
		pub::SpaceObj::GetMass(ci.dwTargetShip, target_mass);

		uint client_ship;
		pub::Player::GetShip(iClientID, client_ship);

		float client_mass;
		pub::SpaceObj::GetMass(client_ship, client_mass);

		// Don't do spin protect unless the hit ship is big
		if (target_mass < set_fSpinProtectMass)
			return;

		// Don't do spin protect unless the hit ship is 2 times larger than the hitter
		if (target_mass < client_mass * 2)
			return;

		Vector V1, V2;
		pub::SpaceObj::GetMotion(ci.dwTargetShip, V1, V2);
		V1.x *= set_fSpinImpulseMultiplier * client_mass;
		V1.y *= set_fSpinImpulseMultiplier * client_mass;
		V1.z *= set_fSpinImpulseMultiplier * client_mass;
		V2.x *= set_fSpinImpulseMultiplier * client_mass;
		V2.y *= set_fSpinImpulseMultiplier * client_mass;
		V2.z *= set_fSpinImpulseMultiplier * client_mass;
		pub::SpaceObj::AddImpulse(ci.dwTargetShip, V1, V2);

	}

	void __stdcall GFGoodBuy(struct SGFGoodBuyInfo const &gbi, unsigned int iClientID)
	{
		returncode = DEFAULT_RETURNCODE;

		/*
		if (Rename::IsLockedShip(iClientID, 2))
		{
			const GoodInfo *packageInfo = GoodList::find_by_id(gbi.iGoodID);
			if (packageInfo->iType == 1)
			{
				if (!arch_is_combinable(gbi.iGoodID))
				{
					PrintUserCmdText(iClientID, L"Pineapple!!!");
					float fItemValue;
					pub::Market::GetPrice(gbi.iBaseID, Arch2Good(gbi.iGoodID), fItemValue);
					wstring wscCharname = Players.GetActiveCharacterName(iClientID);
					HkAddCash(wscCharname, 0+(int)fItemValue);
					returncode = SKIPPLUGINS_NOFUNCTIONCALL;
				}
			}
		}
		*/

		if (PurchaseRestrictions::GFGoodBuy(gbi, iClientID))
		{
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		}
	}

	void __stdcall ReqAddItem(unsigned int goodID, char const *hardpoint, int count, float status, bool mounted, uint iClientID)
	{
		returncode = DEFAULT_RETURNCODE;

		/*if (Rename::IsLockedShip(iClientID, 2))
		{
			string HP(hardpoint);
			if (HP == "BAY")
			{
				//PrintUserCmdText(iClientID, L"Triggered on add cargo");
			}
			else
			{
				PrintUserCmdText(iClientID, L"Buying equipment is not allowed on a locked ship. You will now be kicked.");

				wstring wsccharname = Players.GetActiveCharacterName(iClientID);
				wstring spurdoip;
				HkGetPlayerIP(iClientID, spurdoip);
				AddLog("SHIPLOCK: Attempt to buy item on locked ship %s from IP %s", wstos(wsccharname).c_str(), wstos(spurdoip).c_str());
				ConPrint(L"SHIPLOCK: Attempt to buy item on locked ship %s from IP %s\n", wsccharname.c_str(), spurdoip.c_str());

				HkDelayedKick(iClientID, 1);
				returncode = SKIPPLUGINS_NOFUNCTIONCALL;
				//PrintUserCmdText(iClientID, L"Triggered on add equip");
			}
		}*/

		if (PurchaseRestrictions::ReqAddItem(goodID, hardpoint, count, status, mounted, iClientID))
		{
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		}
	}

	void __stdcall ReqRemoveItem(unsigned short slot, int amount, unsigned int iClientID)
	{
		returncode = DEFAULT_RETURNCODE;

		if (Rename::IsLockedShip(iClientID, 2))
		{
			PurchaseRestrictions::ReqChangeCashHappenedStatus(iClientID, false);

			for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
			{
				if (item->sID == slot)
				{
					string hp = string(item->szHardPoint.value);
					boost::to_upper(hp);
					if (item->bMounted == true && (hp == "BAY" || hp.substr(0, 4) == "HPCM" || hp.substr(0, 18) == "HPSPECIALEQUIPMENT"))
					{
						PrintUserCmdText(iClientID, L"This ship is locked. You can't sell your ID, Armor, or CM/Cloak. You will be kicked to prevent corruption.");
						wstring wsccharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
						wstring spurdoip;
						HkGetPlayerIP(iClientID, spurdoip);
						AddLog("SHIPLOCK: Attempt to sell ID/Armor on locked ship %s from IP %s", wstos(wsccharname).c_str(), wstos(spurdoip).c_str());
						ConPrint(L"SHIPLOCK: Attempt to sell ID/Armor on locked ship %s from IP %s\n", wsccharname.c_str(), spurdoip.c_str());

						CUSTOM_REVERSE_TRANSACTION_STRUCT info;
						info.iClientID = iClientID;
						Plugin_Communication(CUSTOM_REVERSE_TRANSACTION, &info);

						HkDelayedKick(iClientID, 1);

						returncode = SKIPPLUGINS_NOFUNCTIONCALL;
					}
					else if (hp != "BAY" && Rename::IsLockedShip(iClientID, 3))
					{
						PrintUserCmdText(iClientID, L"Selling equipment is not allowed on this ship. You will now be kicked to prevent corruption.");
						wstring wsccharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
						wstring spurdoip;
						HkGetPlayerIP(iClientID, spurdoip);
						AddLog("SHIPLOCK: Attempt to sell item on locked ship %s from IP %s", wstos(wsccharname).c_str(), wstos(spurdoip).c_str());
						ConPrint(L"SHIPLOCK: Attempt to sell item on locked ship %s from IP %s\n", wsccharname.c_str(), spurdoip.c_str());

						CUSTOM_REVERSE_TRANSACTION_STRUCT info;
						info.iClientID = iClientID;
						Plugin_Communication(CUSTOM_REVERSE_TRANSACTION, &info);

						HkDelayedKick(iClientID, 1);
						returncode = SKIPPLUGINS_NOFUNCTIONCALL;
					}
				}
			}
		}

	}

	void __stdcall ReqChangeCash(int iMoneyDiff, unsigned int iClientID)
	{
		if (Rename::IsLockedShip(iClientID, 2))
			PurchaseRestrictions::ReqChangeCashHappenedStatus(iClientID, true);

		returncode = DEFAULT_RETURNCODE;
		if (PurchaseRestrictions::ReqChangeCash(iMoneyDiff, iClientID))
		{
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		}
	}

	void __stdcall ReqSetCash(int iMoney, unsigned int iClientID)
	{
		returncode = DEFAULT_RETURNCODE;
		if (PurchaseRestrictions::ReqSetCash(iMoney, iClientID))
		{
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		}
	}

	void __stdcall ReqEquipment(class EquipDescList const &eqDesc, unsigned int iClientID)
	{
		returncode = DEFAULT_RETURNCODE;

		if (Rename::IsLockedShip(iClientID, 2))
		{
			if (PurchaseRestrictions::ReqChangeCashHappenedStatus(iClientID, false))
				return;

			PrintUserCmdText(iClientID, L"This ship is locked. You can't unmount equipment. You will be kicked to prevent corruption.");
			wstring wsccharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
			wstring spurdoip;
			HkGetPlayerIP(iClientID, spurdoip);
			AddLog("SHIPLOCK: Attempt to unmount equipment on locked ship %s from IP %s", wstos(wsccharname).c_str(), wstos(spurdoip).c_str());
			ConPrint(L"SHIPLOCK: Attempt to unmount equipment on locked ship %s from IP %s\n", wsccharname.c_str(), spurdoip.c_str());

			CUSTOM_REVERSE_TRANSACTION_STRUCT info;
			info.iClientID = iClientID;
			Plugin_Communication(CUSTOM_REVERSE_TRANSACTION, &info);

			HkDelayedKick(iClientID, 1);

			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		}
		/*
		if (Rename::IsLockedShip(iClientID, 2))
		{
			for (EquipDescListItem *item = Players[iClientID].lShadowEquipDescList.pFirst->next;
				item != Players[iClientID].lShadowEquipDescList.pFirst; item = item->next)
			{
					if (string(item->equip.szHardPoint.value) == "BAY")
					{
						PrintUserCmdText(iClientID, L"Triggered on cargo item");
						//PrintUserCmdText(iClientID, L"I like %d", item->equip.get_count());
					}
					else
					{
						PrintUserCmdText(iClientID, L"Triggered on equip item");
					}
			}

			//PrintUserCmdText(iClientID, L"Modifying equipment is not allowed on a locked ship. You will now be kicked.");

			//wstring wsccharname = Players.GetActiveCharacterName(iClientID);
			//wstring spurdoip;
			//HkGetPlayerIP(iClientID, spurdoip);
			//AddLog("SHIPLOCK: Attempt to modify item on locked ship %s from IP %s", wstos(wsccharname).c_str(), wstos(spurdoip).c_str());
			//ConPrint(L"SHIPLOCK: Attempt to modify item on locked ship %s from IP %s\n", wsccharname.c_str(), spurdoip.c_str());

			//HkDelayedKick(iClientID, 1);
			//returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		}
		*/

		if (PurchaseRestrictions::ReqEquipment(eqDesc, iClientID))
		{
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		}
	}

	void __stdcall ReqModifyItem(unsigned short iArchID, char const *Hardpoint, int count, float p4, bool bMounted, unsigned int iClientID)
	{
		returncode = DEFAULT_RETURNCODE;

		if (Rename::IsLockedShip(iClientID, 2))
		{
			PurchaseRestrictions::ReqChangeCashHappenedStatus(iClientID, false);

			if (bMounted == true)
			{
				PrintUserCmdText(iClientID, L"This ship is locked. You can't mount an ID or Armor. You will be kicked to prevent corruption.");

				wstring wsccharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
				wstring spurdoip;
				HkGetPlayerIP(iClientID, spurdoip);
				AddLog("SHIPLOCK: Attempt to mount ID/Armor on locked ship %s from IP %s", wstos(wsccharname).c_str(), wstos(spurdoip).c_str());
				ConPrint(L"SHIPLOCK: Attempt to mount ID/Armor on locked ship %s from IP %s\n", wsccharname.c_str(), spurdoip.c_str());

				HkDelayedKick(iClientID, 1);

				returncode = SKIPPLUGINS_NOFUNCTIONCALL;
			}
			if (bMounted == false)
			{
				PrintUserCmdText(iClientID, L"This ship is locked. You can't unmount your ID or Armor. You will be kicked to prevent corruption.");

				wstring wsccharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
				wstring spurdoip;
				HkGetPlayerIP(iClientID, spurdoip);
				AddLog("SHIPLOCK: Attempt to unmount ID/Armor on locked ship %s from IP %s", wstos(wsccharname).c_str(), wstos(spurdoip).c_str());
				ConPrint(L"SHIPLOCK: Attempt to unmount ID/Armor on locked ship %s from IP %s\n", wsccharname.c_str(), spurdoip.c_str());

				HkDelayedKick(iClientID, 1);

				returncode = SKIPPLUGINS_NOFUNCTIONCALL;
			}
		}
	}

	void __stdcall ReqHullStatus(float fStatus, unsigned int iClientID)
	{
		returncode = DEFAULT_RETURNCODE;
		if (PurchaseRestrictions::ReqHullStatus(fStatus, iClientID))
		{
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		}
	}

	void __stdcall ReqShipArch(unsigned int iArchID, unsigned int iClientID)
	{
		returncode = DEFAULT_RETURNCODE;
		if (PurchaseRestrictions::ReqShipArch(iArchID, iClientID))
		{
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		}
	}

	void __stdcall ReqShipArch_AFTER(unsigned int iArchID, unsigned int iClientID)
	{
		returncode = DEFAULT_RETURNCODE;
		PimpShip::ReqShipArch_AFTER(iArchID, iClientID);
	}

	map<uint, mstime> mapSaveTimes;

	void Timer()
	{
		mstime currTime = GetTimeInMS();
		for (map<uint, mstime>::iterator iter = mapSaveTimes.begin(); iter != mapSaveTimes.end(); ++iter)
		{
			uint iClientID = iter->first;
			if (iter->second != 0 && iter->second < currTime)
			{
				if (HkIsValidClientID(iClientID) && !HkIsInCharSelectMenu(iClientID))
					HkSaveChar(iter->first);
				iter->second = 0;
			}
		}
	}

	// Save after a tractor to prevent cargo duplication loss on crash
	void __stdcall TractorObjects(unsigned int iClientID, struct XTractorObjects const &objs)
	{
		returncode = DEFAULT_RETURNCODE;
		if (mapSaveTimes[iClientID] == 0)
		{
			mapSaveTimes[iClientID] = GetTimeInMS() + 60000;
		}
	}

	// Save after jettison to reduce chance of duplication on crash
	void __stdcall JettisonCargo(unsigned int iClientID, struct XJettisonCargo const &objs)
	{
		if (mapSaveTimes[iClientID] == 0)
		{
			mapSaveTimes[iClientID] = GetTimeInMS() + 60000;
		}
	}

	void __stdcall SetTarget(uint uClientID, struct XSetTarget const &p2)
	{
		returncode = DEFAULT_RETURNCODE;
		Message::SetTarget(uClientID, p2);
	}

	void __stdcall CharacterInfoReq(unsigned int iClientID, bool p2)
	{
		returncode = DEFAULT_RETURNCODE;
		Message::CharacterInfoReq(iClientID, p2);
		AntiJumpDisconnect::CharacterInfoReq(iClientID, p2);
		MiscCmds::CharacterInfoReq(iClientID, p2);
	}

	void __stdcall SubmitChat(struct CHAT_ID cId, unsigned long lP1, void const *rdlReader, struct CHAT_ID cIdTo, int iP2)
	{
		returncode = DEFAULT_RETURNCODE;

		// If we're in a base then reset the base kick time if the 
		// player is chatting to stop the player being kicked.
		if (ClientInfo[cId.iID].iBaseEnterTime)
		{
			ClientInfo[cId.iID].iBaseEnterTime = (uint)time(0);
		}

		// The message subsystem may suppress some chat messages.
		if (Message::SubmitChat(cId, lP1, rdlReader, cIdTo, iP2))
		{
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		}
	}

	void __stdcall GoTradelane(unsigned int iClientID, struct XGoTradelane const &xgt)
	{
		returncode = DEFAULT_RETURNCODE;
		SystemSensor::GoTradelane(iClientID, xgt);
	}

	void __stdcall StopTradelane(unsigned int iClientID, unsigned int p1, unsigned int p2, unsigned int p3)
	{
		returncode = DEFAULT_RETURNCODE;
		SystemSensor::StopTradelane(iClientID, p1, p2, p3);
	}

	void __stdcall SPObjUpdate(struct SSPObjUpdateInfo const &ui, unsigned int iClientID)
	{
		returncode = DEFAULT_RETURNCODE;
		CargoDrop::SPObjUpdate(ui, iClientID);
	}
}

void __stdcall HkCb_SendChat(uint iClientID, uint iTo, uint iSize, void *pRDL)
{
	returncode = DEFAULT_RETURNCODE;

	if (Message::HkCb_SendChat(iClientID, iTo, iSize, pRDL))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
	}
}

int __stdcall HkCB_MissileTorpHit(char *ECX, char *p1, DamageList *dmg)
{
	returncode = DEFAULT_RETURNCODE;

	char *szP;
	memcpy(&szP, ECX + 0x10, 4);
	uint iClientID;
	memcpy(&iClientID, szP + 0xB4, 4);

	if (iClientID)
	{
		HyperJump::MissileTorpHit(iClientID, dmg);
	}
	return 0;
}

void __stdcall RequestBestPath(unsigned int p1, DWORD *p2, int p3)
{
	returncode = SKIPPLUGINS_NOFUNCTIONCALL;
	try
	{
		Server.RequestBestPath(p1, (unsigned char*)p2, p3);
	}
	catch (...)
	{
		AddLog("ERROR: Exception in RequestBestPath p1=%d p2=%08x %08x %08x %08x %08x %08x %08x %08x %08x p3=%08x",
			p1, p2[0], p2[7], p2[3], p2[4], p2[5], p2[8], p2[9], p2[10], p2[12]);
	}
}

typedef bool(*_UserCmdProc)(uint, const wstring &, const wstring &, const wchar_t*);

struct USERCMD
{
	wchar_t *wszCmd;
	_UserCmdProc proc;
	wchar_t *usage;
};

// The user chat commands for this plugin
USERCMD UserCmds[] =
{
	{ L"/pos",			MiscCmds::UserCmd_Pos,			L"Usage: /pos" },
	{ L"/stuck",		MiscCmds::UserCmd_Stuck,		L"Usage: /stuck" },
	{ L"/droprep",		MiscCmds::UserCmd_DropRep,		L"Usage: /droprep" },
	{ L"/resetrep",		MiscCmds::UserCmd_ResetRep,		L"Usage: /resetrep" },
	{ L"/dice",			MiscCmds::UserCmd_Dice,			L"Usage: /dice 1d20 | 1d20+3 | etc."},
	{ L"/roll",			MiscCmds::UserCmd_Dice,			L"Usage: /roll 1d20 | 1d20+3 | etc." },
	{ L"/coin",			MiscCmds::UserCmd_Coin,			L"Usage: /coin" },
	{ L"/pimpship",		PimpShip::UserCmd_PimpShip,		L"Usage: /pimpship" },
	{ L"/showsetup",	PimpShip::UserCmd_ShowSetup,	L"Usage: /showsetup" },
	{ L"/showitems",	PimpShip::UserCmd_ShowItems,	L"Usage: /showitems" },
	{ L"/setitem",		PimpShip::UserCmd_ChangeItem,	L"Usage: /setitem" },
	{ L"/renameme",		Rename::UserCmd_RenameMe,		L"Usage: /renameme <charname> [password]" },
	{ L"/movechar",		Rename::UserCmd_MoveChar,		L"Usage: /movechar <charname> <code>" },
	{ L"/set movecharcode",	Rename::UserCmd_SetMoveCharCode,	L"Usage: /set movecharcode <code>" },
	{ L"/restart",		Restart::UserCmd_Restart,		L"Usage: /restart <faction>" },
	{ L"/showrestarts",	Restart::UserCmd_ShowRestarts,	L"Usage: /showrestarts" },
	{ L"/givecash",		GiveCash::UserCmd_GiveCash,		L"Usage: /givecash <charname> <cash> [anon] [comment] or /gc ..." },
	{ L"/gc",			GiveCash::UserCmd_GiveCash,		L"Usage: /givecash <charname> <cash> [anon] [comment] or /gc ..." },
	{ L"/givecasht",	GiveCash::UserCmd_GiveCashTarget,		L"Usage: /givecasht <cash> [anon] [comment] or /gct ..." },
	{ L"/gct",			GiveCash::UserCmd_GiveCashTarget,		L"Usage: /givecasht <cash> [anon] [comment] or /gct ..." },
	{ L"/sendcash",		GiveCash::UserCmd_GiveCash,		L"Usage: /givecash <charname> <cash> [anon] [comment]"},
	{ L"/set cashcode",	GiveCash::UserCmd_SetCashCode,	L"Usage: /set cashcode <code>"},
	{ L"/showcash",		GiveCash::UserCmd_ShowCash,		L"Usage: /showcash <charname> <code> or /shc ..." },
	{ L"/drc",		GiveCash::UserCmd_DrawCash,		L"Usage: /drawcash <charname> <code> <cash> or /drc ..." },
	{ L"/shc",		GiveCash::UserCmd_ShowCash,		L"Usage: /showcash <charname> <code> or /shc ..." },
	{ L"/drawcash",		GiveCash::UserCmd_DrawCash,		L"Usage: /drawcash <charname> <code> <cash> or /drc ..." },
	{ L"/group",		Message::UserCmd_GroupMsg, L"Usage: /group <message> or /g ..." },
	{ L"/g",			Message::UserCmd_GroupMsg, L"Usage: /group <message> or /g ..." },
	{ L"/local",		Message::UserCmd_LocalMsg, L"Usage: /local <message> or /l ...>" },
	{ L"/l",			Message::UserCmd_LocalMsg, L"Usage: /local <message> or /l ...>" },
	{ L"/system",		Message::UserCmd_SystemMsg, L"Usage: /system <message> or /s ..." },
	{ L"/s",			Message::UserCmd_SystemMsg, L"Usage: /system <message> or /s ..." },
	{ L"/invite",		Message::UserCmd_BuiltInCmdHelp, L"Usage: /invite <charname> or /i ..."},
	{ L"/invite$",		Message::UserCmd_BuiltInCmdHelp, L"Usage: /invite <clientid> or /i$ ..."},
	{ L"/join",			Message::UserCmd_BuiltInCmdHelp, L"Usage: /join <charname> or /j"},
	{ L"/setmsg",		Message::UserCmd_SetMsg, L"Usage: /setmsg <n> <msg text>"},
	{ L"/showmsgs",     Message::UserCmd_ShowMsgs, L"" },
	{ L"/0",		    Message::UserCmd_DRMsg, L"Usage: /n (n=0-9)"},
	{ L"/1",		    Message::UserCmd_DRMsg, L"Usage: /n (n=0-9)"},
	{ L"/2",		    Message::UserCmd_DRMsg, L"Usage: /n (n=0-9)"},
	{ L"/3",		    Message::UserCmd_DRMsg, L"Usage: /n (n=0-9)"},
	{ L"/4",		    Message::UserCmd_DRMsg, L"Usage: /n (n=0-9)"},
	{ L"/5",		    Message::UserCmd_DRMsg, L"Usage: /n (n=0-9)"},
	{ L"/6",		    Message::UserCmd_DRMsg, L"Usage: /n (n=0-9)"},
	{ L"/7",		    Message::UserCmd_DRMsg, L"Usage: /n (n=0-9)"},
	{ L"/8",		    Message::UserCmd_DRMsg, L"Usage: /n (n=0-9)"},
	{ L"/9",		    Message::UserCmd_DRMsg, L"Usage: /n (n=0-9)"},
	{ L"/s0",		    Message::UserCmd_SMsg, L"Usage: /sn (n=0-9)"},
	{ L"/s1",		    Message::UserCmd_SMsg, L"Usage: /sn (n=0-9)"},
	{ L"/s2",		    Message::UserCmd_SMsg, L"Usage: /sn (n=0-9)"},
	{ L"/s3",		    Message::UserCmd_SMsg, L"Usage: /sn (n=0-9)"},
	{ L"/s4",		    Message::UserCmd_SMsg, L"Usage: /sn (n=0-9)"},
	{ L"/s5",		    Message::UserCmd_SMsg, L"Usage: /sn (n=0-9)"},
	{ L"/s6",		    Message::UserCmd_SMsg, L"Usage: /sn (n=0-9)"},
	{ L"/s7",		    Message::UserCmd_SMsg, L"Usage: /sn (n=0-9)"},
	{ L"/s8",		    Message::UserCmd_SMsg, L"Usage: /sn (n=0-9)"},
	{ L"/s9",		    Message::UserCmd_SMsg, L"Usage: /sn (n=0-9)"},
	{ L"/l0",		    Message::UserCmd_LMsg, L"Usage: /ln (n=0-9)"},
	{ L"/l1",		    Message::UserCmd_LMsg, L"Usage: /ln (n=0-9)"},
	{ L"/l2",		    Message::UserCmd_LMsg, L"Usage: /ln (n=0-9)"},
	{ L"/l3",		    Message::UserCmd_LMsg, L"Usage: /ln (n=0-9)"},
	{ L"/l4",		    Message::UserCmd_LMsg, L"Usage: /ln (n=0-9)"},
	{ L"/l5",		    Message::UserCmd_LMsg, L"Usage: /ln (n=0-9)"},
	{ L"/l6",		    Message::UserCmd_LMsg, L"Usage: /ln (n=0-9)"},
	{ L"/l7",		    Message::UserCmd_LMsg, L"Usage: /ln (n=0-9)"},
	{ L"/l8",		    Message::UserCmd_LMsg, L"Usage: /ln (n=0-9)"},
	{ L"/l9",		    Message::UserCmd_LMsg, L"Usage: /ln (n=0-9)"},
	{ L"/g0",		    Message::UserCmd_GMsg, L"Usage: /gn (n=0-9)"},
	{ L"/g1",		    Message::UserCmd_GMsg, L"Usage: /gn (n=0-9)"},
	{ L"/g2",		    Message::UserCmd_GMsg, L"Usage: /gn (n=0-9)"},
	{ L"/g3",		    Message::UserCmd_GMsg, L"Usage: /gn (n=0-9)"},
	{ L"/g4",		    Message::UserCmd_GMsg, L"Usage: /gn (n=0-9)"},
	{ L"/g5",		    Message::UserCmd_GMsg, L"Usage: /gn (n=0-9)"},
	{ L"/g6",		    Message::UserCmd_GMsg, L"Usage: /gn (n=0-9)"},
	{ L"/g7",		    Message::UserCmd_GMsg, L"Usage: /gn (n=0-9)"},
	{ L"/g8",		    Message::UserCmd_GMsg, L"Usage: /gn (n=0-9)"},
	{ L"/g9",		    Message::UserCmd_GMsg, L"Usage: /gn (n=0-9)"},
	{ L"/t0",		    Message::UserCmd_SendToLastTarget, L"Usage: /tn (n=0-9)"},
	{ L"/t1",		    Message::UserCmd_SendToLastTarget, L"Usage: /tn (n=0-9)"},
	{ L"/t2",		    Message::UserCmd_SendToLastTarget, L"Usage: /tn (n=0-9)"},
	{ L"/t3",		    Message::UserCmd_SendToLastTarget, L"Usage: /tn (n=0-9)"},
	{ L"/t4",		    Message::UserCmd_SendToLastTarget, L"Usage: /tn (n=0-9)"},
	{ L"/t5",		    Message::UserCmd_SendToLastTarget, L"Usage: /tn (n=0-9)"},
	{ L"/t6",		    Message::UserCmd_SendToLastTarget, L"Usage: /tn (n=0-9)"},
	{ L"/t7",		    Message::UserCmd_SendToLastTarget, L"Usage: /tn (n=0-9)"},
	{ L"/t8",		    Message::UserCmd_SendToLastTarget, L"Usage: /tn (n=0-9)"},
	{ L"/t9",		    Message::UserCmd_SendToLastTarget, L"Usage: /tn (n=0-9)"},
	{ L"/target",       Message::UserCmd_SendToLastTarget, L"Usage: /target <message> or /t ..." },
	{ L"/t",            Message::UserCmd_SendToLastTarget, L"Usage: /target <message> or /t ..." },
	{ L"/reply",        Message::UserCmd_ReplyToLastPMSender, L"Usage: /reply <message> or /r .." },
	{ L"/r",            Message::UserCmd_ReplyToLastPMSender, L"Usage: /reply <message> or /r ..." },
	{ L"/privatemsg$",	Message::UserCmd_PrivateMsgID, L"Usage: /privatemsg$ <clientid> <messsage> or /pm ..."},
	{ L"/pm$",			Message::UserCmd_PrivateMsgID, L"Usage: /privatemsg$ <clientid> <messsage> or /pm ..."},
	{ L"/privatemsg",	Message::UserCmd_PrivateMsg, L"Usage: /privatemsg <charname> <messsage> or /pm ..."},
	{ L"/pm",			Message::UserCmd_PrivateMsg, L"Usage: /privatemsg <charname> <messsage> or /pm ..."},
	{ L"/factionmsg",	Message::UserCmd_FactionMsg, L"Usage: /factionmsg <tag> <message> or /fm ..."},
	{ L"/fm",			Message::UserCmd_FactionMsg, L"Usage: /factionmsg <tag> <message> or /fm ..."},
	{ L"/invite",	Message::UserCmd_Invite, L"Usage: /invite <name> or /i ..." },
	{ L"/i",			Message::UserCmd_Invite, L"Usage: /invite <name> or /i ..." },
	{ L"/factioninvite",Message::UserCmd_FactionInvite, L"Usage: /factioninvite <tag> or /fi ..."},
	{ L"/fi",			Message::UserCmd_FactionInvite, L"Usage: /factioninvite <tag> or /fi ..."},
	{ L"/lastpm",       Message::UserCmd_ShowLastPMSender, L""},
	{ L"/set chattime", Message::UserCmd_SetChatTime, L"Usage: /set chattime [on|off]"},
	{ L"/set dietime",  Message::UserCmd_SetDeathTime, L"Usage: /set dietime [on|off]"},
	{ L"/help",			Message::UserCmd_CustomHelp, L""},
	{ L"/h",			Message::UserCmd_CustomHelp, L""},
	{ L"/?",			Message::UserCmd_CustomHelp, L""},
	{ L"/commandlist",	Message::UserCmd_CommandList, L""},
	{ L"/mail",			Message::UserCmd_MailShow, L"Usage: /mail <msgnum>"},
	{ L"/maildel",		Message::UserCmd_MailDel, L"Usage: /maildel <msgnum>"},
	{ L"/si",			PlayerInfo::UserCmd_ShowInfo, L"Usage: /showinfo"},
	{ L"/setinfo",		PlayerInfo::UserCmd_SetInfo, L"Usage: /setinfo"},
	{ L"/time",			Message::UserCmd_Time, L""},
	{ L"/time*",		Message::UserCmd_Time, L""},
	{ L"/showinfo",		PlayerInfo::UserCmd_ShowInfo, L"Usage: /showinfo"},
	{ L"/showinfo*",	PlayerInfo::UserCmd_ShowInfo, L"Usage: /showinfo"},
	{ L"/lights",		MiscCmds::UserCmd_Lights, L"Usage: /lights"},
	{ L"/lights*",		MiscCmds::UserCmd_Lights, L"Usage: /lights"},
	//{ L"/selfdestruct",	MiscCmds::UserCmd_SelfDestruct, L"Usage: /selfdestruct"},
	//{ L"/selfdestruct*",MiscCmds::UserCmd_SelfDestruct, L"Usage: /selfdestruct"},
	{ L"/shields",		MiscCmds::UserCmd_Shields, L"Usage: /shields"},
	{ L"/shields*",		MiscCmds::UserCmd_Shields, L"Usage: /shields"},
	//{ L"/ss",		    MiscCmds::UserCmd_Screenshot, L"Usage: /ss"},
	{ L"/survey",		HyperJump::UserCmd_Survey, L"Usage: /survey"},
	{ L"/showcoords",		Message::UserCmd_ShowCoords, L"Usage: /savecoords <n>"},
	{ L"/savecoords",		Message::UserCmd_SaveCoords, L"Usage: /savecoords <n> <text>"},
	{ L"/c0",		Message::UserCmd_LoadCoords, L"Usage: /cn (n=0-9)"},
	{ L"/c1",		Message::UserCmd_LoadCoords, L"Usage: /cn (n=0-9)"},
	{ L"/c2",		Message::UserCmd_LoadCoords, L"Usage: /cn (n=0-9)"},
	{ L"/c3",		Message::UserCmd_LoadCoords, L"Usage: /cn (n=0-9)"},
	{ L"/c4",		Message::UserCmd_LoadCoords, L"Usage: /cn (n=0-9)"},
	{ L"/c5",		Message::UserCmd_LoadCoords, L"Usage: /cn (n=0-9)"},
	{ L"/c6",		Message::UserCmd_LoadCoords, L"Usage: /cn (n=0-9)"},
	{ L"/c7",		Message::UserCmd_LoadCoords, L"Usage: /cn (n=0-9)"},
	{ L"/c8",		Message::UserCmd_LoadCoords, L"Usage: /cn (n=0-9)"},
	{ L"/c9",		Message::UserCmd_LoadCoords, L"Usage: /cn (n=0-9)"},
	{ L"/setcoords",	HyperJump::UserCmd_SetCoords, L"Usage: /setcoords"},
	{ L"/jump",			HyperJump::UserCmd_ActivateJumpDrive, L"Usage: /jump"},
	{ L"/jump*",		HyperJump::UserCmd_ActivateJumpDrive, L"Usage: /jump"},
	{ L"/beacon",		HyperJump::UserCmd_DeployBeacon, L"Usage: /beacon"},
	{ L"/beacon*",		HyperJump::UserCmd_DeployBeacon, L"Usage: /beacon"},
	{ L"/jumpbeacon",		HyperJump::UserCmd_JumpBeacon, L"Usage: /jumpbeacon"},
	{ L"/jumpbeacon*",		HyperJump::UserCmd_JumpBeacon, L"Usage: /jumpbeacon"},
	{ L"/charge",		HyperJump::UserCmd_ChargeJumpDrive, L"Usage: /charge"},
	{ L"/charge*",		HyperJump::UserCmd_ChargeJumpDrive, L"Usage: /charge"},
	{ L"/jumpsys",		HyperJump::UserCmd_ListJumpableSystems, L"Usage: /jumpsys"},
	{ L"/showscan",		SystemSensor::UserCmd_ShowScan, L"Usage: /showscan or /scan <charname>"},
	{ L"/showscan$",	SystemSensor::UserCmd_ShowScan, L"Usage: /showscan$ or /scanid <clientid>"},
	{ L"/scan",			SystemSensor::UserCmd_ShowScan, L"Usage: /showscan or /scan <charname>"},
	{ L"/scanid",		SystemSensor::UserCmd_ShowScan, L"Usage: /showscan$ or /scanid <clientid>"},
	{ L"/net",			SystemSensor::UserCmd_Net, L"Usage: /net [all|jumponly|off]"},
	{ L"/maketag",		Rename::UserCmd_MakeTag, L"Usage: /maketag <tag> <master password> <description>"},
	{ L"/droptag",		Rename::UserCmd_DropTag, L"Usage: /droptag <tag> <master password>"},
	{ L"/settagpass",	Rename::UserCmd_SetTagPass, L"Usage: /settagpass <tag> <master password> <rename password>"}
};

/**
This function is called by FLHook when a user types a chat string. We look at the
string they've typed and see if it starts with one of the above commands. If it
does we try to process it.
*/
bool UserCmd_Process(uint iClientID, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	try
	{
		wstring wscCmdLineLower = ToLower(wscCmd);

		Message::UserCmd_Process(iClientID, wscCmdLineLower);

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
	}
	catch (...)
	{
		AddLog("ERROR: Exception in PlayerCntl::UserCmd_Process(iClientID=%u, wscCmd=%s)", iClientID, wstos(wscCmd).c_str());
		LOG_EXCEPTION;
	}
	return false;
}

std::list<uint> npcs;

void UserCmd_Help(uint iClientID, const wstring &wscParam)
{
	returncode = DEFAULT_RETURNCODE;
	PrintUserCmdText(iClientID, L"/pos");
	PrintUserCmdText(iClientID, L"/stuck");
	PrintUserCmdText(iClientID, L"/droprep");
	PrintUserCmdText(iClientID, L"/roll 1d20");
	PrintUserCmdText(iClientID, L"/coin");

	if (!set_bEnableRenameMe)
	{
		PrintUserCmdText(iClientID, L"/renameme <charname>");
	}

	if (!set_bEnableMoveChar)
	{
		PrintUserCmdText(iClientID, L"/movechar <charname> <code>");
		PrintUserCmdText(iClientID, L"/set movecharcode <code>");
	}

	if (!set_bEnableRestart)
	{
		PrintUserCmdText(iClientID, L"/restart <faction>");
		PrintUserCmdText(iClientID, L"/showrestarts");
	}

	if (set_bEnableGiveCash)
	{
		PrintUserCmdText(iClientID, L"/givecash <charname> <cash>");
		PrintUserCmdText(iClientID, L"/drawcash <charname> <code> <cash>");
		PrintUserCmdText(iClientID, L"/showcash <charname> <code>");
		PrintUserCmdText(iClientID, L"/set cashcode <code>");
	}

	PrintUserCmdText(iClientID, L"/showmsgs");
	PrintUserCmdText(iClientID, L"/setmsg");
	PrintUserCmdText(iClientID, L"/n  (n=0-9)");
	PrintUserCmdText(iClientID, L"/ln (n=0-9)");
	PrintUserCmdText(iClientID, L"/gn (n=0-9)");
	PrintUserCmdText(iClientID, L"/tn (n=0-9)");
	PrintUserCmdText(iClientID, L"/target or /t");
	PrintUserCmdText(iClientID, L"/reply or /r");
	PrintUserCmdText(iClientID, L"/privatemsg or /pm"),
		PrintUserCmdText(iClientID, L"/privatemsg$ or /pm$"),
		PrintUserCmdText(iClientID, L"/factionmsg or /fm"),
		PrintUserCmdText(iClientID, L"/factioninvite or /fi");
	PrintUserCmdText(iClientID, L"/set chattime");
	PrintUserCmdText(iClientID, L"/time");
	PrintUserCmdText(iClientID, L"/mail");
	PrintUserCmdText(iClientID, L"/maildel");
}


pub::AI::SetPersonalityParams HkMakePersonality() {

	pub::AI::SetPersonalityParams p;
	p.state_graph = pub::StateGraph::get_state_graph("FIGHTER", pub::StateGraph::TYPE_STANDARD);
	p.state_id = true;

	p.personality.EvadeDodgeUse.evade_dodge_style_weight[0] = 0.4f;
	p.personality.EvadeDodgeUse.evade_dodge_style_weight[1] = 0.0f;
	p.personality.EvadeDodgeUse.evade_dodge_style_weight[2] = 0.4f;
	p.personality.EvadeDodgeUse.evade_dodge_style_weight[3] = 0.2f;
	p.personality.EvadeDodgeUse.evade_dodge_cone_angle = 1.5708f;
	p.personality.EvadeDodgeUse.evade_dodge_interval_time = 10.0f;
	p.personality.EvadeDodgeUse.evade_dodge_time = 1.0f;
	p.personality.EvadeDodgeUse.evade_dodge_distance = 75.0f;
	p.personality.EvadeDodgeUse.evade_activate_range = 100.0f;
	p.personality.EvadeDodgeUse.evade_dodge_roll_angle = 1.5708f;
	p.personality.EvadeDodgeUse.evade_dodge_waggle_axis_cone_angle = 1.5708f;
	p.personality.EvadeDodgeUse.evade_dodge_slide_throttle = 1.0f;
	p.personality.EvadeDodgeUse.evade_dodge_turn_throttle = 1.0f;
	p.personality.EvadeDodgeUse.evade_dodge_corkscrew_roll_flip_direction = true;
	p.personality.EvadeDodgeUse.evade_dodge_interval_time_variance_percent = 0.5f;
	p.personality.EvadeDodgeUse.evade_dodge_cone_angle_variance_percent = 0.5f;
	p.personality.EvadeDodgeUse.evade_dodge_direction_weight[0] = 0.25f;
	p.personality.EvadeDodgeUse.evade_dodge_direction_weight[1] = 0.25f;
	p.personality.EvadeDodgeUse.evade_dodge_direction_weight[2] = 0.25f;
	p.personality.EvadeDodgeUse.evade_dodge_direction_weight[3] = 0.25f;

	p.personality.EvadeBreakUse.evade_break_roll_throttle = 1.0f;
	p.personality.EvadeBreakUse.evade_break_time = 1.0f;
	p.personality.EvadeBreakUse.evade_break_interval_time = 10.0f;
	p.personality.EvadeBreakUse.evade_break_afterburner_delay = 0.0f;
	p.personality.EvadeBreakUse.evade_break_turn_throttle = 1.0f;
	p.personality.EvadeBreakUse.evade_break_direction_weight[0] = 1.0f;
	p.personality.EvadeBreakUse.evade_break_direction_weight[1] = 1.0f;
	p.personality.EvadeBreakUse.evade_break_direction_weight[2] = 1.0f;
	p.personality.EvadeBreakUse.evade_break_direction_weight[3] = 1.0f;
	p.personality.EvadeBreakUse.evade_break_style_weight[0] = 1.0f;
	p.personality.EvadeBreakUse.evade_break_style_weight[1] = 1.0f;
	p.personality.EvadeBreakUse.evade_break_style_weight[2] = 1.0f;

	p.personality.BuzzHeadTowardUse.buzz_min_distance_to_head_toward = 500.0f;
	p.personality.BuzzHeadTowardUse.buzz_min_distance_to_head_toward_variance_percent = 0.25f;
	p.personality.BuzzHeadTowardUse.buzz_max_time_to_head_away = 1.0f;
	p.personality.BuzzHeadTowardUse.buzz_head_toward_engine_throttle = 1.0f;
	p.personality.BuzzHeadTowardUse.buzz_head_toward_turn_throttle = 1.0f;
	p.personality.BuzzHeadTowardUse.buzz_head_toward_roll_throttle = 1.0f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_turn_throttle = 1.0f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_cone_angle = 1.5708f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_cone_angle_variance_percent = 0.5f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_waggle_axis_cone_angle = 0.3491f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_roll_angle = 1.5708f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_interval_time = 10.0f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_interval_time_variance_percent = 0.5f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_direction_weight[0] = 0.25f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_direction_weight[1] = 0.25f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_direction_weight[2] = 0.25f;
	p.personality.BuzzHeadTowardUse.buzz_dodge_direction_weight[3] = 0.25f;
	p.personality.BuzzHeadTowardUse.buzz_head_toward_style_weight[0] = 0.33f;
	p.personality.BuzzHeadTowardUse.buzz_head_toward_style_weight[1] = 0.33f;
	p.personality.BuzzHeadTowardUse.buzz_head_toward_style_weight[2] = 0.33f;

	p.personality.BuzzPassByUse.buzz_distance_to_pass_by = 1000.0f;
	p.personality.BuzzPassByUse.buzz_pass_by_time = 1.0f;
	p.personality.BuzzPassByUse.buzz_break_direction_cone_angle = 1.5708f;
	p.personality.BuzzPassByUse.buzz_break_turn_throttle = 1.0f;
	p.personality.BuzzPassByUse.buzz_drop_bomb_on_pass_by = true;
	p.personality.BuzzPassByUse.buzz_break_direction_weight[0] = 1.0f;
	p.personality.BuzzPassByUse.buzz_break_direction_weight[1] = 1.0f;
	p.personality.BuzzPassByUse.buzz_break_direction_weight[2] = 1.0f;
	p.personality.BuzzPassByUse.buzz_break_direction_weight[3] = 1.0f;
	p.personality.BuzzPassByUse.buzz_pass_by_style_weight[2] = 1.0f;

	p.personality.TrailUse.trail_lock_cone_angle = 0.0873f;
	p.personality.TrailUse.trail_break_time = 0.5f;
	p.personality.TrailUse.trail_min_no_lock_time = 0.1f;
	p.personality.TrailUse.trail_break_roll_throttle = 1.0f;
	p.personality.TrailUse.trail_break_afterburner = true;
	p.personality.TrailUse.trail_max_turn_throttle = 1.0f;
	p.personality.TrailUse.trail_distance = 100.0f;

	p.personality.StrafeUse.strafe_run_away_distance = 100.0f;
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

	p.personality.GunUse.gun_fire_interval_time = 0.0f;
	p.personality.GunUse.gun_fire_interval_variance_percent = 0.0f;
	p.personality.GunUse.gun_fire_burst_interval_time = 2.0f;
	p.personality.GunUse.gun_fire_burst_interval_variance_percent = 0.0f;
	p.personality.GunUse.gun_fire_no_burst_interval_time = 0.0f;
	p.personality.GunUse.gun_fire_accuracy_cone_angle = 0.00001f;
	p.personality.GunUse.gun_fire_accuracy_power = 100.0f;
	p.personality.GunUse.gun_range_threshold = 1.0f;
	p.personality.GunUse.gun_target_point_switch_time = 0.0f;
	p.personality.GunUse.fire_style = 0;
	p.personality.GunUse.auto_turret_interval_time = 2.0f;
	p.personality.GunUse.auto_turret_burst_interval_time = 2.0f;
	p.personality.GunUse.auto_turret_no_burst_interval_time = 0.0f;
	p.personality.GunUse.auto_turret_burst_interval_variance_percent = 0.0f;
	p.personality.GunUse.gun_range_threshold_variance_percent = 0.3f;
	p.personality.GunUse.gun_fire_accuracy_power_npc = 100.0f;

	p.personality.MineUse.mine_launch_interval = 8.0f;
	p.personality.MineUse.mine_launch_cone_angle = 0.7854f;
	p.personality.MineUse.mine_launch_range = 200.0f;

	p.personality.MissileUse.missile_launch_interval_time = 0.0f;
	p.personality.MissileUse.missile_launch_interval_variance_percent = 0.5f;
	p.personality.MissileUse.missile_launch_range = 800.0f;
	p.personality.MissileUse.missile_launch_cone_angle = 0.01745f;
	p.personality.MissileUse.missile_launch_allow_out_of_range = false;

	p.personality.DamageReaction.evade_break_damage_trigger_percent = 1.0f;
	p.personality.DamageReaction.evade_dodge_more_damage_trigger_percent = 0.25f;
	p.personality.DamageReaction.engine_kill_face_damage_trigger_percent = 1.0f;
	p.personality.DamageReaction.engine_kill_face_damage_trigger_time = 0.2f;
	p.personality.DamageReaction.roll_damage_trigger_percent = 0.4f;
	p.personality.DamageReaction.roll_damage_trigger_time = 0.2f;
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
	p.personality.Job.loot_preference = 7;
	p.personality.Job.combat_drift_distance = 25000;
	p.personality.Job.attack_order[0].distance = 5000;
	p.personality.Job.attack_order[0].type = 11;
	p.personality.Job.attack_order[0].flag = 15;
	p.personality.Job.attack_order[1].type = 12;

	return p;
}

#define IS_CMD(a) !wscCmd.compare(L##a)

bool ExecuteCommandString_Callback(CCmds* cmds, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;


	if (IS_CMD("smiteall"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		MiscCmds::AdminCmd_SmiteAll(cmds);
		return true;
	}
	else if (IS_CMD("bob"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		MiscCmds::AdminCmd_Bob(cmds, cmds->ArgCharname(1));
		return true;
	}

	else if (IS_CMD("playmusic"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		MiscCmds::AdminCmd_PlayMusic(cmds, cmds->ArgStrToEnd(1));
		return true;
	}
	else if (IS_CMD("playsound"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		MiscCmds::AdminCmd_PlaySound(cmds, cmds->ArgStrToEnd(1));
		return true;
	}
	else if (IS_CMD("playnnm"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		MiscCmds::AdminCmd_PlayNNM(cmds, cmds->ArgStrToEnd(1));
		return true;
	}
	else if (IS_CMD("beam"))
	{
		if (HyperJump::AdminCmd_Beam(cmds, cmds->ArgCharname(1), cmds->ArgStrToEnd(2)))
		{
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
			return true;
		}
	}
	else if (IS_CMD("pull"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		HyperJump::AdminCmd_Pull(cmds, cmds->ArgCharname(1));
		return true;
	}
	else if (IS_CMD("move"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		HyperJump::AdminCmd_Move(cmds, cmds->ArgFloat(1), cmds->ArgFloat(2), cmds->ArgFloat(3));
		return true;
	}
	else if (IS_CMD("chase"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		HyperJump::AdminCmd_Chase(cmds, cmds->ArgCharname(1));
		return true;
	}
	else if (IS_CMD("lrs"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		HyperJump::AdminCmd_ListRestrictedShips(cmds);
		return true;
	}
	else if (IS_CMD("makecoord"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		HyperJump::AdminCmd_MakeCoord(cmds);
		return true;
	}
	else if (IS_CMD("authchar"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		IPBans::AdminCmd_AuthenticateChar(cmds, cmds->ArgStr(1));
		return true;
	}
	else if (IS_CMD("reloadbans"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		IPBans::AdminCmd_ReloadBans(cmds);
		return true;
	}
	else if (IS_CMD("setaccmovecode"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		Rename::AdminCmd_SetAccMoveCode(cmds, cmds->ArgCharname(1), cmds->ArgStr(2));
		return true;
	}
	else if (IS_CMD("rotatelogs"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		if (fLogDebug)
		{
			fclose(fLogDebug);
			::MoveFileA(sDebugLog.c_str(), string(sDebugLog + ".old").c_str());
			_unlink(sDebugLog.c_str());
			fLogDebug = fopen(sDebugLog.c_str(), "wt");
		}

		if (fLog)
		{
			fclose(fLog);
			::MoveFileA("./flhook_logs/FLHook.log", "./flhook_logs/FLHook.log.old");
			_unlink("./flhook_logs/FLHook.log");
			fLog = fopen("./flhook_logs/FLHook.log", "wt");
		}

		cmds->Print(L"OK\n");
		return true;
	}
	else if (IS_CMD("pm") || IS_CMD("privatemsg"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		Message::AdminCmd_SendMail(cmds, cmds->ArgCharname(1), cmds->ArgStrToEnd(2));
		return true;
	}
	else if (IS_CMD("pm") || IS_CMD("privatemsg"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		Message::AdminCmd_SendMail(cmds, cmds->ArgCharname(1), cmds->ArgStrToEnd(2));
		return true;
	}
	else if (IS_CMD("showtags"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		Rename::AdminCmd_ShowTags(cmds);
		return true;
	}
	else if (IS_CMD("addtag"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		Rename::AdminCmd_AddTag(cmds, cmds->ArgStr(1), cmds->ArgStr(2), cmds->ArgStrToEnd(3));
		return true;
	}
	else if (IS_CMD("droptag"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		Rename::AdminCmd_DropTag(cmds, cmds->ArgStr(1));
		return true;
	}
	else if (IS_CMD("reloadlockedships"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		Rename::ReloadLockedShips();
		return true;
	}
	else if (IS_CMD("sethp"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		MiscCmds::AdminCmd_SetHP(cmds, cmds->ArgCharname(1), cmds->ArgUInt(2));
		return true;
	}
	else if (IS_CMD("setfuse"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		MiscCmds::AdminCmd_SetFuse(cmds, cmds->ArgCharname(1), cmds->ArgStr(2));
		return true;
	}
	else if (IS_CMD("sethpfuse"))
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		MiscCmds::AdminCmd_SetHPFuse(cmds, cmds->ArgCharname(1), cmds->ArgUInt(2), cmds->ArgStr(3));
		return true;
	}
	else if (IS_CMD("unsetfuse"))
		{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		MiscCmds::AdminCmd_UnsetFuse(cmds, cmds->ArgCharname(1), cmds->ArgStr(2));
		return true;
	}
	return false;
}

/** Admin help command callback */
void CmdHelp_Callback(CCmds* classptr)
{
	returncode = DEFAULT_RETURNCODE;
	classptr->Print(L"move x, y, z\n");
	classptr->Print(L"pull <charname>\n");
	classptr->Print(L"chase <charname>\n");
	classptr->Print(L"smiteall [die]\n");
	classptr->Print(L"testbot <system> <testtime>\n");
	classptr->Print(L"authchar <charname>\n");
	classptr->Print(L"reloadbans\n");
	classptr->Print(L"setaccmovecode <charname> <code>\n");
	classptr->Print(L"rotatelogs\n");
	classptr->Print(L"privatemsg|pm <charname> <message>\n");

	classptr->Print(L"showtags\n");
	classptr->Print(L"addtag <tag> <password>\n");
	classptr->Print(L"droptag <tag> <password>\n");
}

void __stdcall Elapse_Time(float delta)
{
	returncode = SKIPPLUGINS_NOFUNCTIONCALL;
	if (delta < 0.0001f)
	{
		AddLog("ERROR: Elapse time correction delta=%f", delta);
		ConPrint(L"Elapse time correction delta=%f\n", delta);
		delta = 0.0001f;
	}

	try
	{
		Server.ElapseTime(delta);
	}
	catch (...)
	{
		AddLog("ERROR: Exception in Server.ElapseTime(delta=%f)", delta);
		LOG_EXCEPTION;
	}
}

void Plugin_Communication_CallBack(PLUGIN_MESSAGE msg, void* data)
{
	returncode = DEFAULT_RETURNCODE;

	if (msg == CLIENT_CLOAK_INFO)
	{
		CLIENT_CLOAK_STRUCT* info = reinterpret_cast<CLIENT_CLOAK_STRUCT*>(data);

		HyperJump::ClientCloakCallback(info);
	}
	else if (msg == CUSTOM_JUMP)
	{
		CUSTOM_JUMP_STRUCT* info = reinterpret_cast<CUSTOM_JUMP_STRUCT*>(data);

		uint iClientID = HkGetClientIDByShip(info->iShipID);
		if (iClientID)
		{
			SystemSensor::JumpInComplete(info->iSystemID, info->iShipID, iClientID);
		}
	}
	return;
}

/** Functions to hook */
EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "Player Control Plugin by cannon 2.0";
	p_PI->sShortName = "playercntl";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ClearClientInfo, PLUGIN_ClearClientInfo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkTimer, PLUGIN_HkTimerCheckKick, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SendDeathMsg, PLUGIN_SendDeathMsg, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::Startup, PLUGIN_HkIServerImpl_Startup, 10));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::Startup_AFTER, PLUGIN_HkIServerImpl_Startup_AFTER, 10));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::Login, PLUGIN_HkIServerImpl_Login, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::RequestEvent, PLUGIN_HkIServerImpl_RequestEvent, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::PlayerLaunch, PLUGIN_HkIServerImpl_PlayerLaunch, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::PlayerLaunch_AFTER, PLUGIN_HkIServerImpl_PlayerLaunch_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::BaseEnter, PLUGIN_HkIServerImpl_BaseEnter, 0));
	// check causes lag: p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::BaseEnter_AFTER, PLUGIN_HkIServerImpl_BaseEnter_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::LocationEnter, PLUGIN_HkIServerImpl_LocationEnter, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::DisConnect, PLUGIN_HkIServerImpl_DisConnect, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::CharacterSelect_AFTER, PLUGIN_HkIServerImpl_CharacterSelect_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::JumpInComplete_AFTER, PLUGIN_HkIServerImpl_JumpInComplete_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::SystemSwitchOutComplete, PLUGIN_HkIServerImpl_SystemSwitchOutComplete, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::SPObjCollision, PLUGIN_HkIServerImpl_SPObjCollision, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::GFGoodBuy, PLUGIN_HkIServerImpl_GFGoodBuy, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::ReqAddItem, PLUGIN_HkIServerImpl_ReqAddItem, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::ReqModifyItem, PLUGIN_HkIServerImpl_ReqModifyItem, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::ReqRemoveItem, PLUGIN_HkIServerImpl_ReqRemoveItem, 16));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::ReqChangeCash, PLUGIN_HkIServerImpl_ReqChangeCash, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::ReqSetCash, PLUGIN_HkIServerImpl_ReqSetCash, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::ReqEquipment, PLUGIN_HkIServerImpl_ReqEquipment, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::ReqHullStatus, PLUGIN_HkIServerImpl_ReqHullStatus, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::ReqShipArch, PLUGIN_HkIServerImpl_ReqShipArch, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::ReqShipArch_AFTER, PLUGIN_HkIServerImpl_ReqShipArch_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::TractorObjects, PLUGIN_HkIServerImpl_TractorObjects_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::JettisonCargo, PLUGIN_HkIServerImpl_JettisonCargo_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::SetTarget, PLUGIN_HkIServerImpl_SetTarget, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::CharacterInfoReq, PLUGIN_HkIServerImpl_CharacterInfoReq, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::SubmitChat, PLUGIN_HkIServerImpl_SubmitChat, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::SPObjUpdate, PLUGIN_HkIServerImpl_SPObjUpdate, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::GoTradelane, PLUGIN_HkIServerImpl_GoTradelane, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::StopTradelane, PLUGIN_HkIServerImpl_StopTradelane, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::CreateNewCharacter, PLUGIN_HkIServerImpl_CreateNewCharacter, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::DestroyCharacter, PLUGIN_HkIServerImpl_DestroyCharacter, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::CreateGuided, PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_CREATEGUIDED, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIEngine::Dock_Call, PLUGIN_HkCb_Dock_Call, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkCb_SendChat, PLUGIN_HkCb_SendChat, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Help, PLUGIN_UserCmd_Help, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ExecuteCommandString_Callback, PLUGIN_ExecuteCommandString_Callback, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CmdHelp_Callback, PLUGIN_CmdHelp_Callback, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkCB_MissileTorpHit, PLUGIN_HkCB_MissileTorpHit, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&RequestBestPath, PLUGIN_HkIServerImpl_RequestBestPath, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Plugin_Communication_CallBack, PLUGIN_Plugin_Communication, 0));
	//	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SPMunitionCollision, PLUGIN_HkIServerImpl_SPMunitionCollision, 0));
	return p_PI;
}
