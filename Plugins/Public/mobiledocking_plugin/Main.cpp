#include "Main.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function handling changing systems when undocking from a carrier
void JumpToLocation(uint client, uint system, Vector pos, Matrix ornt)
{
	mapDeferredJumps[client].system = system;
	mapDeferredJumps[client].pos = pos;
	mapDeferredJumps[client].rot = ornt;

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
		fprintf(file, "Autobanned by Docking Module\n");
		fclose(file);
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

// Load the configuration
void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\mobiledocking.cfg";

	// Load generic settings
	set_iPluginDebug = IniGetI(scPluginCfgFile, "General", "Debug", 0);

	struct PlayerData *pd = 0;

	// Load all of the ships which have logged out while being docked
	while (pd = Players.traverse_active(pd))
	{
		if (!HkIsInCharSelectMenu(pd->iOnlineID))
			LoadDockInfo(pd->iOnlineID);
	}
}

bool UserCmd_Process(uint client, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;
	if(wscCmd.find(L"/listdocked") == 0)
	{
		if(clients[client].mapDockedShips.size() == 0)
		{
			PrintUserCmdText(client, L"No ships currently docked");
		}
		else
		{
			PrintUserCmdText(client, L"Docked ships:");
			for(map<wstring, wstring>::iterator i = clients[client].mapDockedShips.begin();
				i != clients[client].mapDockedShips.end(); ++i)
			{
				PrintUserCmdText(client, i->first);
			}
		}
	}
	else if(wscCmd.find(L"/allowdock")==0)
	{
		//If we're not in space, then ignore the request
		uint iShip;
		pub::Player::GetShip(client, iShip);
		if (!iShip)
			return true;

		//If there is no ship currently targeted, then ignore the request
		uint iTargetShip;
		pub::SpaceObj::GetTarget(iShip, iTargetShip);
		if (!iTargetShip)
			return true;

		// If the target is not a player ship, or if the ship is too far away, ignore
		uint iTargetClientID = HkGetClientIDByShip(iTargetShip);
		if (!iTargetClientID || HkDistance3DByShip(iShip, iTargetShip) > 1000.0f)
		{
			PrintUserCmdText(client, L"Ship is out of range");
			return true;
		}

		// Find the docking request. If none, ignore.
		if (mapPendingDockingRequests.find(iTargetClientID) == mapPendingDockingRequests.end())
		{
			PrintUserCmdText(client, L"No pending docking requests for this ship");
			return true;
		}

		// Check that there is an empty docking module
		if(clients[client].mapDockedShips.size() >= clients[client].iDockingModules)
		{
			mapPendingDockingRequests.erase(iTargetClientID);
			PrintUserCmdText(client, L"No free docking modules available.");
			return true;
		}

		// The client is free to dock, erase from the pending list and handle
		mapPendingDockingRequests.erase(iTargetClientID);

		string scProxyBase = HkGetPlayerSystemS(client) + "_proxy_base";
		uint iBaseID;
		if(pub::GetBaseID(iBaseID, scProxyBase.c_str()) == -4)
		{
			PrintUserCmdText(client, L"No proxy base in system detected. Contact an administrator about this please.");
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

		// Land the ship on the proxy base
		pub::Player::ForceLand(iTargetClientID, iBaseID);
		PrintUserCmdText(client, L"Ship docked");
		return true;
	}
	return false;
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	srand((uint)time(0));
	// If we're being loaded from the command line while FLHook is running then
	// set_scCfgFile will not be empty so load the settings as FLHook only
	// calls load settings on FLHook startup and .rehash.
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		if (set_scCfgFile.length()>0)
			LoadSettings();
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
	}
	return true;
}
