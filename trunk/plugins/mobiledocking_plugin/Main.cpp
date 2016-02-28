/**
 Mobile Docking Plugin for FLHook-Plugin
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
#include "Main.h"

void LoadDockInfo(uint client);
void SaveDockInfo(uint client);
void UpdateDockInfo(const wstring &charname, uint iSystem, Vector pos, Matrix rot);

void SendSetBaseInfoText2(UINT client, const wstring &message);
void SendResetMarketOverride(UINT client);

map<uint, CLIENT_DATA> clients;

//conn hash
uint connSystemID = CreateID("LI06");

struct DEFERREDJUMPS
{
	UINT system;
	Vector pos;
	Matrix ornt;
};
static map<UINT, DEFERREDJUMPS> mapDeferredJumps;

struct DOCKING_REQUEST
{
	uint iTargetClientID;
};
map<uint, DOCKING_REQUEST> mapPendingDockingRequests;

/// The debug mode
static int set_iPluginDebug = 0;

/// The distance to undock from carrier
static int set_iMobileDockOffset = 100;

/// A return code to indicate to FLHook if we want the hook processing to continue.
PLUGIN_RETURNCODE returncode;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void JumpToLocation(uint client, uint system, Vector pos, Matrix ornt)
{
	mapDeferredJumps[client].system = system;
	mapDeferredJumps[client].pos = pos;
	mapDeferredJumps[client].ornt = ornt;

	// Send the jump command to the client. The client will send a system switch out complete
	// event which we intercept to set the new starting positions.
	PrintUserCmdText(client, L" ChangeSys %u", system);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LogCheater(uint client, const wstring &reason)
{
	CAccount *acc = Players.FindAccountFromClientID(client);

	if (!HkIsValidClientID(client) || !acc)
	{
		AddLog("ERROR: invalid parameter in log cheater, clientid=%u acc=%08x reason=%s", client, acc, wstos(reason).c_str());
		return;
	}

	// Set the kick timer to kick this player. We do this to break potential
	// stack corruption.
	HkDelayedKick(client, 1);

	// Ban the account.
	flstr *flStr = CreateWString(acc->wszAccID);
	Players.BanAccount(*flStr, true);
	FreeWString(flStr);

	// Overwrite the ban file so that it contains the ban reason
	wstring wscDir;
	HkGetAccountDirName(acc, wscDir);
	string scBanPath = scAcctPath + wstos(wscDir) + "\\banned";
	FILE *file = fopen(scBanPath.c_str(), "wb");
	if (file)
	{
		fprintf(file, "Autobanned by Marketfucker Type-6\n");
		fclose(file);
	}
}

void UpdateDockedShips(uint client)
{
	uint tgt;
	uint system;
	Vector pos, vec, bvec;
	Matrix rot;

	pub::Player::GetShip(client, tgt);
	if (!tgt)
	{
		uint iBaseID;
		pub::Player::GetBase(client, iBaseID);
		if (!iBaseID)
			return;

		Universe::IBase *base = Universe::get_base(iBaseID);
		tgt = base->lSpaceObjID;

		pub::SpaceObj::GetSystem(tgt, system);
		pub::SpaceObj::GetLocation(tgt, pos, rot);

		float rad, brad;
		pub::SpaceObj::GetRadius(tgt, rad, vec );
		pub::SpaceObj::GetBurnRadius(tgt, brad, bvec );
		if (rad < brad)
			rad = brad;

		pos.x += rot.data[0][1]*(rad+set_iMobileDockOffset);
		pos.y += rot.data[1][1]*(rad+set_iMobileDockOffset);
		pos.z += rot.data[2][1]*(rad+set_iMobileDockOffset);
	}
	else
	{
		pub::SpaceObj::GetSystem(tgt, system);
		pub::SpaceObj::GetLocation(tgt, pos, rot);

		pos.x += rot.data[0][1]*set_iMobileDockOffset;
		pos.y += rot.data[1][1]*set_iMobileDockOffset;
		pos.z += rot.data[2][1]*set_iMobileDockOffset;
	}


	// For each docked ship, update it's last location to reflect that of the carrier
	if (clients[client].mapDockedShips.size())
	{
		for (map<wstring, wstring>::iterator i = clients[client].mapDockedShips.begin();
					i != clients[client].mapDockedShips.end(); ++i)
		{
			uint iDockedClientID = HkGetClientIdFromCharname(i->first);
			if (iDockedClientID)
			{
				clients[iDockedClientID].iCarrierSystem = system;
				clients[iDockedClientID].vCarrierLocation = pos;
				clients[iDockedClientID].mCarrierLocation = rot;
				SaveDockInfo(iDockedClientID);
			}
			else
			{
				UpdateDockInfo(i->first, system, pos, rot);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Clear client info when a client connects.
void ClearClientInfo(uint client)
{
	returncode = DEFAULT_RETURNCODE;
	clients.erase(client);
	mapDeferredJumps.erase(client);
	mapPendingDockingRequests.erase(client);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Load the configuration
void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	// The path to the configuration file.
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\mobiledocking.cfg";

	// Load generic settings
	set_iPluginDebug = IniGetI(scPluginCfgFile, "General", "Debug", 0);

	struct PlayerData *pd = 0;
	
	while (pd = Players.traverse_active(pd))
	{
		if (!HkIsInCharSelectMenu(pd->iOnlineID))
			LoadDockInfo(pd->iOnlineID);
	}
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	srand((uint)time(0));
	// If we're being loaded from the command line while FLHook is running then
	// set_scCfgFile will not be empty so load the settings as FLHook only
	// calls load settings on FLHook startup and .rehash.
	if(fdwReason == DLL_PROCESS_ATTACH)
	{
		if (set_scCfgFile.length()>0)
			LoadSettings();
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
	}
	return true;
}

bool UserCmd_Process(uint client, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;
	if (wscCmd.find(L"/listdocked")==0)
	{
		if (clients[client].mapDockedShips.size() == 0)
		{
			PrintUserCmdText(client, L"No ships docked");
		}
		else
		{
			PrintUserCmdText(client, L"Docked ships:");
			for (map<wstring, wstring>::iterator i = clients[client].mapDockedShips.begin();
					i != clients[client].mapDockedShips.end(); ++i)
			{
				PrintUserCmdText(client, i->first);
			}
		}
	}
	else if (wscCmd.find(L"/jettisonship")==0)
	{
		wstring charname = Trim(GetParam(wscCmd, ' ', 1));
		if (!charname.size())
		{
			PrintUserCmdText(client, L"Usage: /jettisonship <charname>");
			return true;
		}
	
		if (clients[client].mapDockedShips.find(charname)
			== clients[client].mapDockedShips.end())
		{
			PrintUserCmdText(client, L"Ship not docked");
			return true;
		}

		UpdateDockedShips(client);

		// Send a system switch to force the ship to launch. Do nothing
		// if the ship is in space for some reason.
		uint iDockedClientID = HkGetClientIdFromCharname(charname);
		if (iDockedClientID)
		{
			uint ship;
			pub::Player::GetShip(iDockedClientID, ship);
			if (!ship)
			{
				JumpToLocation(iDockedClientID,
					clients[iDockedClientID].iCarrierSystem,
					clients[iDockedClientID].vCarrierLocation,
					clients[iDockedClientID].mCarrierLocation);
			}
		}

		// Remove the ship from the list
		clients[client].mapDockedShips.erase(charname);
		SaveDockInfo(client);
		PrintUserCmdText(client, L"Ship jettisoned");
		return true;
	}
	// The allow dock command accepts a docking request.
	else if (wscCmd.find(L"/allowdock")==0)
	{
		// If not in space then ignore the request.
		uint iShip;
		pub::Player::GetShip(client, iShip);
		if (!iShip)
			return true;

		// If no target then ignore the request.
		uint iTargetShip;
		pub::SpaceObj::GetTarget(iShip, iTargetShip);
		if (!iTargetShip)
			return true;
		
		// If target is not player ship or ship is too far away then ignore the request.
		uint iTargetClientID = HkGetClientIDByShip(iTargetShip);
		if (!iTargetClientID || HkDistance3DByShip(iShip, iTargetShip) > 1000.0f)
		{
			PrintUserCmdText(client, L"Ship is out of range");
			return true;
		}

		// Find the docking request. If there is no docking request then ignore this
		// command
		if (mapPendingDockingRequests.find(iTargetClientID) == mapPendingDockingRequests.end()
			|| mapPendingDockingRequests[iTargetClientID].iTargetClientID != client)
		{
			PrintUserCmdText(client, L"No pending docking requests for this ship");
			return true;
		}

		// Check that the target ship has an empty docking module. Report the error
		if (clients[client].mapDockedShips.size() >= clients[client].iDockingModules)
		{
			mapPendingDockingRequests.erase(iTargetClientID);
			PrintUserCmdText(client, L"No free docking capacity");
			return true;
		}

		// Delete the docking request and dock the player.
		mapPendingDockingRequests.erase(iTargetClientID);

		string scProxyBase = HkGetPlayerSystemS(client) + "_proxy_base";
		uint iBaseID;
		if (pub::GetBaseID(iBaseID, scProxyBase.c_str()) == -4)
		{
			PrintUserCmdText(client, L"No proxy base, contact administrator");
			return true;
		}

		// Save the carrier info
		wstring charname = (const wchar_t*)Players.GetActiveCharacterName(iTargetClientID);
		clients[client].mapDockedShips[charname] = charname;
		SaveDockInfo(client);
		
		// Save the docking ship info
		clients[iTargetClientID].mobile_docked = true;
		clients[iTargetClientID].wscDockedWithCharname = (const wchar_t*)Players.GetActiveCharacterName(client);
		if (clients[iTargetClientID].iLastBaseID != 0)
			clients[iTargetClientID].iLastBaseID = Players[iTargetClientID].iLastBaseID;
		pub::SpaceObj::GetSystem(iShip, clients[iTargetClientID].iCarrierSystem);
		pub::SpaceObj::GetLocation(iShip, clients[iTargetClientID].vCarrierLocation, clients[iTargetClientID].mCarrierLocation);
		SaveDockInfo(iTargetClientID);
		
		// Land the ship on the proxy base.
		pub::Player::ForceLand(iTargetClientID, iBaseID);
		PrintUserCmdText(client, L"Ship docked");
		return true;
	}
	return false;
}

// If this is a docking request at a player ship then process it.
int __cdecl Dock_Call(unsigned int const &iShip, unsigned int const &iBaseID, int iCancel, enum DOCK_HOST_RESPONSE response)
{
	returncode = DEFAULT_RETURNCODE;

	UINT client = HkGetClientIDByShip(iShip);
	if (client)
	{
		// If no target then ignore the request.
		uint iTargetShip;
		pub::SpaceObj::GetTarget(iShip, iTargetShip);
		if (!iTargetShip)
			return 0;

		uint iType;
		pub::SpaceObj::GetType(iTargetShip, iType);
		if (iType != OBJ_FREIGHTER)
			return 0;

		// If target is not player ship or ship is too far away then ignore the request.
		uint iTargetClientID = HkGetClientIDByShip(iTargetShip);
		if (!iTargetClientID || HkDistance3DByShip(iShip, iTargetShip) > 1000.0f)
		{
			PrintUserCmdText(client, L"Ship is out of range");
			return 0;
		}

		// Check that the target ship has an empty docking module. Report the error
		if (clients[iTargetClientID].mapDockedShips.size() >= clients[iTargetClientID].iDockingModules)
		{
			PrintUserCmdText(client, L"Target ship has no free docking capacity");
			return 0;
		}

		// Check that the requesting ship is of the appropriate size to dock.
		CShip* cship = (CShip*)HkGetEqObjFromObjRW((IObjRW*)HkGetInspect(client));
		if (cship->shiparch()->fHoldSize > 275)
		{
			PrintUserCmdText(client, L"Target ship is too small");
			return 0;
		}

		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		// Create a docking request and send a notification to the target ship.
		mapPendingDockingRequests[client].iTargetClientID = iTargetClientID;
		PrintUserCmdText(iTargetClientID, L"%s is requesting to dock, authorise with /allowdock", Players.GetActiveCharacterName(client));
		PrintUserCmdText(client, L"Docking request sent to %s", Players.GetActiveCharacterName(iTargetClientID));
		return -1;
	}
	return 0;
}

void __stdcall CharacterSelect_AFTER(struct CHARACTER_ID const &cId,unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
	mapPendingDockingRequests.erase(client);
	LoadDockInfo(client);
}

bool IsShipDockedOnCarrier(wstring &carrier_charname, wstring &docked_charname)
{
	uint client = HkGetClientIdFromCharname(carrier_charname);
	if (client != -1)
	{
		return clients[client].mapDockedShips.find(docked_charname) != clients[client].mapDockedShips.end();
	}
	else
	{
		return false;
	}
}

void __stdcall BaseEnter(uint iBaseID, uint client)
{
	returncode = DEFAULT_RETURNCODE;

	// Update the location of any docked ships.
	if (clients[client].mapDockedShips.size())
	{
		UpdateDockedShips(client);
	}

	if (clients[client].mobile_docked)
	{
		// Clear the market. We don't support transfers in this version.
		SendResetMarketOverride(client);

		// Set the base name
		wstring status = L"<RDL><PUSH/>";
		status += L"<TEXT>" + XMLText(clients[client].wscDockedWithCharname) + L"</TEXT><PARA/><PARA/>";
		status += L"<POP/></RDL>";
		SendSetBaseInfoText2(client, status);

		// Check to see that the carrier thinks this ship is docked to it.
		// If it isn't then eject the ship to space.
		wstring charname = (const wchar_t*)Players.GetActiveCharacterName(client);
		if (!IsShipDockedOnCarrier(clients[client].wscDockedWithCharname, charname))
		{
			JumpToLocation(client,
				clients[client].iCarrierSystem,
				clients[client].vCarrierLocation,
				clients[client].mCarrierLocation);
			return;
		}
	}

}

void __stdcall BaseExit(uint iBaseID, uint client)
{
	returncode = DEFAULT_RETURNCODE;
	LoadDockInfo(client);

	if (clients[client].mobile_docked)
	{
		// Clear the market. We don't support transfers in this version.
		SendResetMarketOverride(client);
	}
}

void __stdcall PlayerLaunch(unsigned int iShip, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;

	if (clients[client].mobile_docked)
	{
		// Update the location of the carrier and remove the docked ship from the carrier
		uint carrier_client = HkGetClientIdFromCharname(clients[client].wscDockedWithCharname);
		if (carrier_client != -1)
		{
			UpdateDockedShips(carrier_client);
			clients[carrier_client].mapDockedShips.erase((const wchar_t*)Players.GetActiveCharacterName(client));
		}

		// Jump the ship to the last location of the carrier and allow
		// the undock to proceed.
		JumpToLocation(client,
			clients[client].iCarrierSystem,
			clients[client].vCarrierLocation,
			clients[client].mCarrierLocation);

		clients[client].mobile_docked = false;
		clients[client].wscDockedWithCharname = L"";
		SaveDockInfo(client);

		//Conn exploiting check

		if (Players[client].iSystemID == connSystemID)
		{
			wstring playerName = (const wchar_t*)Players.GetActiveCharacterName(client);
			list<CARGO_INFO> cargo;
			int holdSize = 0;

			HkEnumCargo(playerName, cargo, holdSize);

			for (list<CARGO_INFO>::const_iterator it = cargo.begin(); it != cargo.end(); ++it)
			{
				const CARGO_INFO & item = *it;

				bool flag = false;
				pub::IsCommodity(item.iArchID, flag);

				// Some commodity present.
				if (flag)
				{
					pub::Player::SendNNMessage(client, pub::GetNicknameId("nnv_anomaly_detected"));
					wstring wscMsgU = L"MF: %name has been permabanned. (Type 6)";
					wscMsgU = ReplaceStr(wscMsgU, L"%name", playerName.c_str());

					HkMsgU(wscMsgU);

					wstring wscMsgLog = L"<%sender> was permabanned for undocking from a docking module with cargo in hold. Type 6.";
					wscMsgLog = ReplaceStr(wscMsgLog, L"%sender", playerName.c_str());

					LogCheater(client, wscMsgLog);
				}
			}
		}

		
	}
}

void __stdcall PlayerLaunch_AFTER(unsigned int iShip, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
}

void SystemSwitchOutComplete(unsigned int iShip, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
	static PBYTE SwitchOut = 0;
	if (!SwitchOut)
	{
		SwitchOut = (PBYTE)hModServer + 0xf600;

		DWORD dummy;
		VirtualProtect(SwitchOut+0xd7, 200, PAGE_EXECUTE_READWRITE, &dummy);
	}

	// Patch the system switch out routine to put the ship in a
	// system of our choosing.
	if (mapDeferredJumps.find(client) != mapDeferredJumps.end())
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		SwitchOut[0x0d7] = 0xeb;				// ignore exit object
		SwitchOut[0x0d8] = 0x40;
		SwitchOut[0x119] = 0xbb;				// set the destination system
		*(PDWORD)(SwitchOut+0x11a) = mapDeferredJumps[client].system;
		SwitchOut[0x266] = 0x45;				// don't generate warning
		*(float*)(SwitchOut+0x2b0) = mapDeferredJumps[client].pos.z;		// set entry location
		*(float*)(SwitchOut+0x2b8) = mapDeferredJumps[client].pos.y;
		*(float*)(SwitchOut+0x2c0) = mapDeferredJumps[client].pos.x;
		*(float*)(SwitchOut+0x2c8) = mapDeferredJumps[client].ornt.data[2][2];
		*(float*)(SwitchOut+0x2d0) = mapDeferredJumps[client].ornt.data[1][1];
		*(float*)(SwitchOut+0x2d8) = mapDeferredJumps[client].ornt.data[0][0];
		*(float*)(SwitchOut+0x2e0) = mapDeferredJumps[client].ornt.data[2][1];
		*(float*)(SwitchOut+0x2e8) = mapDeferredJumps[client].ornt.data[2][0];
		*(float*)(SwitchOut+0x2f0) = mapDeferredJumps[client].ornt.data[1][2];
		*(float*)(SwitchOut+0x2f8) = mapDeferredJumps[client].ornt.data[1][0];
		*(float*)(SwitchOut+0x300) = mapDeferredJumps[client].ornt.data[0][2];
		*(float*)(SwitchOut+0x308) = mapDeferredJumps[client].ornt.data[0][1];
		*(PDWORD)(SwitchOut+0x388) = 0x03ebc031;		// ignore entry object
		mapDeferredJumps.erase(client);
		
		pub::SpaceObj::SetInvincible(iShip, false, false, 0);
		Server.SystemSwitchOutComplete(iShip,client);

		// Unpatch the code.
		SwitchOut[0x0d7] = 0x0f;
		SwitchOut[0x0d8] = 0x84;
		SwitchOut[0x119] = 0x87;
		*(PDWORD)(SwitchOut+0x11a) = 0x1b8;
		*(PDWORD)(SwitchOut+0x25d) = 0x1cf7f;
		SwitchOut[0x266] = 0x1a;
		*(float*)(SwitchOut+0x2b0) =
			*(float*)(SwitchOut+0x2b8) =
			*(float*)(SwitchOut+0x2c0) = 0;
		*(float*)(SwitchOut+0x2c8) =
			*(float*)(SwitchOut+0x2d0) =
			*(float*)(SwitchOut+0x2d8) = 1;
		*(float*)(SwitchOut+0x2e0) =
			*(float*)(SwitchOut+0x2e8) =
			*(float*)(SwitchOut+0x2f0) =
			*(float*)(SwitchOut+0x2f8) =
			*(float*)(SwitchOut+0x300) =
			*(float*)(SwitchOut+0x308) = 0;
		*(PDWORD)(SwitchOut+0x388) = 0xcf8b178b;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall DisConnect(unsigned int client, enum EFLConnection p2)
{
	returncode = DEFAULT_RETURNCODE;
	UpdateDockedShips(client);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall CharacterInfoReq(unsigned int client, bool p2)
{
	returncode = DEFAULT_RETURNCODE;
	UpdateDockedShips(client);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall GFGoodSell(struct SGFGoodSellInfo const &gsi, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;

	if (clients[client].mobile_docked)
	{
		returncode = SKIPPLUGINS;

		PrintUserCmdText(client, L"ERR: Ship will not accept goods");
		clients[client].reverse_sell = true;
		return;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall ReqRemoveItem(unsigned short slot, int count, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;

	if (clients[client].mobile_docked)
	{
		returncode = SKIPPLUGINS;
		
		if (clients[client].reverse_sell)
		{
			int hold_size;
			HkEnumCargo((const wchar_t*)Players.GetActiveCharacterName(client), clients[client].cargo, hold_size);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall ReqRemoveItem_AFTER(unsigned short iID, int count, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;

	if (clients[client].mobile_docked)
	{
		returncode = SKIPPLUGINS;

		if (clients[client].reverse_sell)
		{
			clients[client].reverse_sell = false;

			foreach (clients[client].cargo, CARGO_INFO, ci)
			{
				if (ci->iID == iID)
				{
					Server.ReqAddItem(ci->iArchID, ci->hardpoint.value, count, ci->fStatus, ci->bMounted, client);						
					return;
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall GFGoodBuy(struct SGFGoodBuyInfo const &gbi, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;

	// If the client is in a player controlled base
	if (clients[client].mobile_docked)
	{
		PrintUserCmdText(client, L"ERR Base will not sell goods");
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		clients[client].stop_buy = true;
		return;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall ReqAddItem(unsigned int good, char const *hardpoint, int count, float fStatus, bool bMounted, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
	if (clients[client].mobile_docked)
	{
		returncode = SKIPPLUGINS;

		if (clients[client].stop_buy)
		{
			clients[client].stop_buy = false;
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Ignore cash commands from the client when we're in a player base.
void __stdcall ReqChangeCash(int cash, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
	if (clients[client].mobile_docked)
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Ignore cash commands from the client when we're in a player base.
void __stdcall ReqSetCash(int cash, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
	if (clients[client].mobile_docked)
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall ReqEquipment(class EquipDescList const &edl, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
	if (clients[client].mobile_docked)
		returncode = SKIPPLUGINS;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall ShipDestroyed(DamageList *_dmg, DWORD *ecx, uint kill)
{
	returncode = DEFAULT_RETURNCODE;
	
	CShip *cship = (CShip*)ecx[4];
	uint client = cship->GetOwnerPlayer();
	if (kill)
	{
		if (client)
		{
			// If this is a carrier then drop all docked ships into space.
			if (clients[client].mapDockedShips.size())
			{
				// Update the coordinates
				UpdateDockedShips(client);
				
				// Send a system switch to force the ship to launch
				for (map<wstring, wstring>::iterator i = clients[client].mapDockedShips.begin();
					i != clients[client].mapDockedShips.end(); ++i)
				{
					uint iDockedClientID = HkGetClientIdFromCharname(i->first);
					if (iDockedClientID)
					{
						JumpToLocation(iDockedClientID,
							clients[iDockedClientID].iCarrierSystem,
							clients[iDockedClientID].vCarrierLocation,
							clients[iDockedClientID].mCarrierLocation);
					}
				}

				// Add clear the list.
				clients[client].mapDockedShips.clear();
				SaveDockInfo(client);
			}
			// If this was last docked at a carrier then set the last base to the to
			// last real base the ship docked at.
			else if (clients[client].iLastBaseID)
			{
				Players[client].iLastBaseID = clients[client].iLastBaseID;
				clients[client].iLastBaseID = 0;
			}
		} 
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "Mobile Docking Plugin by cannon";
	p_PI->sShortName = "dock";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ClearClientInfo, PLUGIN_ClearClientInfo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch, PLUGIN_HkIServerImpl_PlayerLaunch, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch_AFTER, PLUGIN_HkIServerImpl_PlayerLaunch_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SystemSwitchOutComplete, PLUGIN_HkIServerImpl_SystemSwitchOutComplete, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterSelect_AFTER, PLUGIN_HkIServerImpl_CharacterSelect_AFTER, 0));	
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&DisConnect, PLUGIN_HkIServerImpl_DisConnect,0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterInfoReq, PLUGIN_HkIServerImpl_CharacterInfoReq,0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ShipDestroyed, PLUGIN_ShipDestroyed,0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseEnter, PLUGIN_HkIServerImpl_BaseEnter, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseExit, PLUGIN_HkIServerImpl_BaseExit, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Dock_Call, PLUGIN_HkCb_Dock_Call, 0));

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&GFGoodSell, PLUGIN_HkIServerImpl_GFGoodSell, 15));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ReqRemoveItem, PLUGIN_HkIServerImpl_ReqRemoveItem, 15));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ReqRemoveItem_AFTER, PLUGIN_HkIServerImpl_ReqRemoveItem, 15));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&GFGoodBuy, PLUGIN_HkIServerImpl_GFGoodBuy, 15));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ReqAddItem, PLUGIN_HkIServerImpl_ReqAddItem, 15));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ReqChangeCash, PLUGIN_HkIServerImpl_ReqChangeCash, 15));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ReqSetCash, PLUGIN_HkIServerImpl_ReqSetCash, 15));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ReqEquipment, PLUGIN_HkIServerImpl_ReqEquipment, 11));
	
	return p_PI;
}
