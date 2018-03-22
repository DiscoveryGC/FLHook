#include "Main.h"

PLUGIN_RETURNCODE returncode;
map<uint, CLIENT_DATA> mobiledockClients;
map<uint, uint> mapPendingDockingRequests;
vector<uint> dockingModuleEquipmentIds;

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
	FILE *file = fopen(scBanPath.c_str(), "wbe");
	if (file)
	{
		fprintf(file, "Autobanned by Docking Module\n");
		fclose(file);
	}
}

bool IsShipDockedOnCarrier(wstring &carrier_charname, wstring &docked_charname)
{
	const uint client = HkGetClientIdFromCharname(carrier_charname);
	if (client != -1)
	{
		return mobiledockClients[client].mapDockedShips.find(docked_charname) != mobiledockClients[client].mapDockedShips.end();
	}
	else
	{
		return false;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Clear client info when a client disconnects.
void ClearClientInfo(uint client)
{
	returncode = DEFAULT_RETURNCODE;
	mobiledockClients.erase(client);
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

	struct PlayerData *pd = 0;

	// Load all of the ships which have logged out while being docked
	while ((pd = Players.traverse_active(pd)))
	{
		if (!HkIsInCharSelectMenu(pd->iOnlineID))
			LoadDockInfo(pd->iOnlineID);
	}

	//@@TODO Add support for defining multiple docking modules in the configuration file
	// Add the current available docking module to the list of available arches
	dockingModuleEquipmentIds.push_back(CreateID("dsy_docking_module_1"));
}

void __stdcall BaseExit(uint iBaseID, uint iClientID)
{

	//Set the players docking module count to 0, update the list to the proper amount shortly afterwards
	mobiledockClients[iClientID].iDockingModules = 0;

	// Check to see if the vessel undocking currently has a docking module equipped
	for(list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
	{
		if(find(dockingModuleEquipmentIds.begin(), dockingModuleEquipmentIds.end(), item->iArchID) != dockingModuleEquipmentIds.end())
		{
			if(item->bMounted)
			{
				mobiledockClients[iClientID].iDockingModules++;
			}
		}
	}

	PrintUserCmdText(iClientID, L"You have %i modules mounted.", mobiledockClients[iClientID].iDockingModules);

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
		const uint iTargetClientID = HkGetClientIDByShip(iTargetShip);
		if (!iTargetClientID || HkDistance3DByShip(iShip, iTargetShip) > 1000.0f)
		{
			PrintUserCmdText(client, L"Ship is out of range");
			return 0;
		}

		// Check that the target ship has an empty docking module. Report the error
		if (mobiledockClients[iTargetClientID].mapDockedShips.size() >= mobiledockClients[iTargetClientID].iDockingModules)
		{
			PrintUserCmdText(client, L"Target ship has no free docking capacity");
			return 0;
		}

		// Check that the requesting ship is of the appropriate size to dock.
		CShip* cship = dynamic_cast<CShip*>(HkGetEqObjFromObjRW(reinterpret_cast<IObjRW*>(HkGetInspect(client))));
		if (cship->shiparch()->fHoldSize > 275)
		{
			PrintUserCmdText(client, L"Target ship is too small");
			return 0;
		}

		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		// Create a docking request and send a notification to the target ship.
		mapPendingDockingRequests[client] = iTargetClientID;
		PrintUserCmdText(iTargetClientID, L"%s is requesting to dock, authorise with /allowdock", Players.GetActiveCharacterName(client));
		PrintUserCmdText(client, L"Docking request sent to %s", Players.GetActiveCharacterName(iTargetClientID));
		return -1;
	}
	return 0;
}

bool UserCmd_Process(uint client, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;
	if(wscCmd.find(L"/listdocked") == 0)
	{
		if(mobiledockClients[client].mapDockedShips.empty())
		{
			PrintUserCmdText(client, L"No ships currently docked");
		}
		else
		{
			PrintUserCmdText(client, L"Docked ships:");
			for(map<wstring, wstring>::iterator i = mobiledockClients[client].mapDockedShips.begin();
				i != mobiledockClients[client].mapDockedShips.end(); ++i)
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
		if(mobiledockClients[client].mapDockedShips.size() >= mobiledockClients[client].iDockingModules)
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
		mobiledockClients[client].mapDockedShips[charname] = charname;
		SaveDockInfo(client, mobiledockClients[client]);

		// Save the docking ship info
		mobiledockClients[iTargetClientID].mobile_docked = true;
		mobiledockClients[iTargetClientID].wscDockedWithCharname = (const wchar_t*)Players.GetActiveCharacterName(client);
		if (mobiledockClients[iTargetClientID].iLastBaseID != 0)
			mobiledockClients[iTargetClientID].iLastBaseID = Players[iTargetClientID].iLastBaseID;
		pub::SpaceObj::GetSystem(iShip, mobiledockClients[iTargetClientID].iCarrierSystem);
		pub::SpaceObj::GetLocation(iShip, mobiledockClients[iTargetClientID].vCarrierLocation, mobiledockClients[iTargetClientID].mCarrierLocation);
		SaveDockInfo(iTargetClientID, mobiledockClients[iTargetClientID]);

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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "Mobile Docking Plugin";
	p_PI->sShortName = "dock";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ClearClientInfo, PLUGIN_ClearClientInfo, 0));

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Dock_Call, PLUGIN_HkCb_Dock_Call, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseExit, PLUGIN_HkIServerImpl_BaseExit, 0));

	return p_PI;
}