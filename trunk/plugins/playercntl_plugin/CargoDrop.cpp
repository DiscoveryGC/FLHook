// Player Control plugin for FLHookPlugin
// Feb 2010 by Cannon
//
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

#include <windows.h>
#include <stdio.h>
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
#include "Main.h"

namespace CargoDrop
{
	wstring set_wscDisconnectInSpaceMsg = L"%player is attempting to engage cloaking device";

	/// If true report disconnecting players.
	bool set_bReportDisconnectingPlayers = true;

	/// If true kill players who disconnect while interacting with other players.
	bool set_bKillDisconnectingPlayers = false;

	/// If true damage and loot players who disconnect while interacting with other players.
	bool set_bLootDisconnectingPlayers = true;

	/// The container to contain loot in when it's dropped by the ship.
	static int set_iLootCrateID = 0;

	/// Drop the player ship bits 'n pieces x this amount.
	static float set_fHullFct = 0.1f;

	/// The item representing ship bits.
	static int set_iHullItem1ID = 0;

	/// The item representing ship bits.
	static int set_iHullItem2ID = 0;

	/// list of items that are not dropped when the ship is F1'd looted.
	static map<uint, uint> set_mapNoLootItems;

	static float set_fDisconnectingPlayersRange = 5000.0f;

	class INFO
	{
	public:
		INFO() : bF1DisconnectProcessed(false), dLastTimestamp(0) {}
		bool bF1DisconnectProcessed;
		double dLastTimestamp;
		Vector vLastPosition;
		Quaternion vLastDir;
	};
	static map<uint, INFO> mapInfo;

	void CargoDrop::LoadSettings(const string &scPluginCfgFile)
	{
		set_bReportDisconnectingPlayers = IniGetB(scPluginCfgFile, "General", "ReportDisconnectingPlayers", true);
		set_bKillDisconnectingPlayers = IniGetB(scPluginCfgFile, "General", "KillDisconnectingPlayers", false);
		set_bLootDisconnectingPlayers = IniGetB(scPluginCfgFile, "General", "LootDisconnectingPlayers", false);
		set_fDisconnectingPlayersRange = IniGetF(scPluginCfgFile, "General", "DisconnectingPlayersRange", 5000.0f);

		set_fHullFct = ToFloat(stows(IniGetS(scPluginCfgFile, "General", "HullDropFactor", "0.1")));
		set_iLootCrateID = CreateID(IniGetS(scPluginCfgFile, "General", "CargoDropContainer","lootcrate_ast_loot_metal").c_str());
		set_iHullItem1ID = CreateID(IniGetS(scPluginCfgFile, "General", "HullDrop1NickName", "commodity_super_alloys").c_str());
		set_iHullItem2ID = CreateID(IniGetS(scPluginCfgFile, "General", "HullDrop2NickName", "commodity_engine_components").c_str());
		set_wscDisconnectInSpaceMsg = stows(IniGetS(scPluginCfgFile, "General", "DisconnectMsg", "%player is attempting to engage cloaking device"));

		// Read the no loot items list (item-nick)
		list<INISECTIONVALUE> lstItems;
		IniGetSection(scPluginCfgFile, "NoLootItems", lstItems);
		set_mapNoLootItems.clear();
		foreach (lstItems, INISECTIONVALUE, iter)
		{
			uint itemID = CreateID(iter->scKey.c_str());
			set_mapNoLootItems[itemID] = itemID;
		}
	}

	void CargoDrop::Timer()
	{
		// Disconnecting while interacting checks.
		for (map<uint, INFO>::iterator iter = mapInfo.begin(); iter!=mapInfo.end(); iter++)
		{
			int iClientID = iter->first;

			// If selecting a character or invalid, do nothing.
			if (!HkIsValidClientID(iClientID) || HkIsInCharSelectMenu(iClientID))
				continue;

			// If not in space, do nothing
			uint iShip;
			pub::Player::GetShip(iClientID, iShip);
			if (!iShip)
				continue;

			if (ClientInfo[iClientID].tmF1Time || ClientInfo[iClientID].tmF1TimeDisconnect)
			{
				wstring wscCharname = (const wchar_t*) Players.GetActiveCharacterName(iClientID);

				// Drain the ship's shields.
				pub::SpaceObj::DrainShields(iShip);

				// Simulate an obj update to stop the ship in space.
				SSPObjUpdateInfo ui;
				iter->second.dLastTimestamp += 1.0;
				ui.fTimestamp = iter->second.dLastTimestamp;
				ui.cState = 0;
				ui.throttle = 0;
				ui.vPos = iter->second.vLastPosition;
				ui.vDir = iter->second.vLastDir;
				Server.SPObjUpdate(ui, iClientID);

				if (!iter->second.bF1DisconnectProcessed)
				{
					iter->second.bF1DisconnectProcessed = true;			

					// Send disconnect report to all ships in scanner range.
					if (set_bReportDisconnectingPlayers)
					{
						wstring wscMsg = set_wscDisconnectInSpaceMsg;
						wscMsg = ReplaceStr(wscMsg, L"%time", GetTimeString(set_bLocalTime));
						wscMsg = ReplaceStr(wscMsg, L"%player", wscCharname);
						PrintLocalUserCmdText(iClientID, wscMsg, set_fDisconnectingPlayersRange);
					}

					// Drop the player's cargo.
					if (set_bLootDisconnectingPlayers && IsInRange(iClientID, set_fDisconnectingPlayersRange))
					{
						uint iSystem = 0;
						pub::Player::GetSystem(iClientID, iSystem);
						uint iShip = 0;
						pub::Player::GetShip(iClientID, iShip);  
						Vector vLoc = { 0.0f, 0.0f, 0.0f };
						Matrix mRot = { 0.0f, 0.0f, 0.0f };
						pub::SpaceObj::GetLocation(iShip, vLoc, mRot);
						vLoc.x += 30.0;

						list<CARGO_INFO> lstCargo;
						int iRemainingHoldSize = 0;
						if (HkEnumCargo(wscCharname, lstCargo, iRemainingHoldSize)==HKE_OK)
						{
							foreach (lstCargo, CARGO_INFO, item)
							{
								if (!item->bMounted && set_mapNoLootItems.find(item->iArchID)==set_mapNoLootItems.end())
								{
									HkRemoveCargo(wscCharname, item->iID, item->iCount);
									Server.MineAsteroid(iSystem, vLoc, set_iLootCrateID, item->iArchID, item->iCount, iClientID);
								}
							}
						}
						HkSaveChar(wscCharname);
					}

					// Kill if other ships are in scanner range.
					if (set_bKillDisconnectingPlayers && IsInRange(iClientID, set_fDisconnectingPlayersRange))
					{
						uint iShip = 0;
						pub::Player::GetShip(iClientID, iShip);
						pub::SpaceObj::SetRelativeHealth(iShip, 0.0f);
					}
				}
			}
		}
	}


	/// Hook for ship distruction. It's easier to hook this than the PlayerDeath one.
	/// Drop a percentage of cargo + some loot representing ship bits.
	void CargoDrop::SendDeathMsg(const wstring &wscMsg, uint iSystem, uint iClientIDVictim, uint iClientIDKiller)
	{	
		// If player ship loot dropping is enabled then check for a loot drop.
		if (set_fHullFct==0.0f)
			return;

		list<CARGO_INFO> lstCargo;
		int iRemainingHoldSize;
		if (HkEnumCargo((const wchar_t*) Players.GetActiveCharacterName(iClientIDVictim), lstCargo, iRemainingHoldSize)!=HKE_OK)
			return;

		int iShipSizeEst = iRemainingHoldSize;
		foreach (lstCargo, CARGO_INFO, iter)
		{
			if (!(iter->bMounted))
			{
				iShipSizeEst += iter->iCount;
			}
		}

		int iHullDrop = (int)(set_fHullFct*(float)iShipSizeEst);
		if (iHullDrop>0)
		{
			uint iShip; 
			pub::Player::GetShip(iClientIDVictim, iShip);  
			Vector myLocation;
			Matrix myRot;
			pub::SpaceObj::GetLocation(iShip, myLocation, myRot);
			myLocation.x += 30.0;

			if (set_iPluginDebug)
				ConPrint(L"NOTICE: player control cargo drop in system %08x at %f,%f,%f for ship size of iShipSizeEst=%d iHullDrop=%d\n", iSystem, myLocation.x, myLocation.y, myLocation.z, iShipSizeEst, iHullDrop);

			Server.MineAsteroid(iSystem, myLocation, set_iLootCrateID, set_iHullItem1ID, iHullDrop, iClientIDVictim);
			Server.MineAsteroid(iSystem, myLocation, set_iLootCrateID, set_iHullItem2ID, (int)(0.5*(float)iHullDrop), iClientIDVictim);
		}
	}

	void CargoDrop::ClearClientInfo(unsigned int iClientID)
	{
		mapInfo.erase(iClientID);
	}

	void CargoDrop::SPObjUpdate(struct SSPObjUpdateInfo const &ui, unsigned int iClientID)
	{
		mapInfo[iClientID].dLastTimestamp = ui.fTimestamp;
		mapInfo[iClientID].vLastPosition = ui.vPos;
		mapInfo[iClientID].vLastDir = ui.vDir;
	}
}