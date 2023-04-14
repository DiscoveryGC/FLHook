/**
 * Mobile Docking Plugin for FLHook
 * Initial Version by Cannon
 * Using some work written by Alley
 * Rework by Remnant
 */

#include "Main.h"
#include <ctime>

PLUGIN_RETURNCODE returncode;
map<uint, uint> mapPendingDockingRequests;
vector<uint> dockingModuleEquipmentIds;
map<uint, CLIENT_DATA> mobiledockClients;

map<wstring, CARRIERINFO> nameToCarrierInfoMap;
map<wstring, DOCKEDCRAFTINFO> nameToDockedInfoMap;
map<uint, CARRIERINFO*> idToCarrierInfoMap;
map<uint, DOCKEDCRAFTINFO*> idToDockedInfoMap;

uint saveFrequency = 7200; // once every 2 hours, possibly a heavy operation so doesn't have to happen often
uint forgetCarrierDataInSeconds = 31556926; // a year

// Above how much cargo capacity, should a ship be rejected as a docking user?
int cargoCapacityLimit = 275;

float mobileDockingRange = 500.0f;

// Load the configuration
void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	// The path to the data file.
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	// Create the directory if it doesn't exist
	string moddir = string(datapath) + "\\Accts\\MultiPlayer\\docking_module\\";
	CreateDirectoryA(moddir.c_str(), 0);

	string scCarrierDataFile = moddir + "mobile_docking.ini";

	// Plugin configuration
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\dockingmodules.cfg";

	int dockingModAmount = 0;
	int carrierCount = 0;
	int dockedCount = 0;
	INI_Reader ini;
	if (ini.open(scPluginCfgFile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("Config"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("allowedmodule"))
					{
						dockingModuleEquipmentIds.push_back(CreateID(ini.get_value_string()));
						dockingModAmount++;
					}
					else if (ini.is_value("cargo_capacity_limit"))
					{
						cargoCapacityLimit = ini.get_value_int(0);
					}
					else if (ini.is_value("save_frequency"))
					{
						saveFrequency = ini.get_value_int(0);
					}
					else if (ini.is_value("carrier_data_wipe_period"))
					{
						forgetCarrierDataInSeconds = ini.get_value_int(0);
					}
					else if (ini.is_value("mobileDockingRange"))
					{
						mobileDockingRange = ini.get_value_float(0);
					}
				}
			}
		}
		ini.close();
	}

	if (ini.open(scCarrierDataFile.c_str(), false))
	{
		time_t curTime = time(0);
		while (ini.read_header())
		{
			if (ini.is_header("CarrierData"))
			{
				CARRIERINFO ci;
				wstring carrierName;
				boolean doLoad = true;
				while (ini.read_value() && doLoad)
				{
					if (ini.is_value("carrier"))
					{
						carrierName = stows(ini.get_value_string(0));
						const auto& accData = HkGetAccountByCharname(carrierName);
						// don't load renamed/deleted ships
						if (!accData)
						{
							doLoad = false;
						}
					}
					else if (ini.is_value("lastLogin"))
					{
						if (forgetCarrierDataInSeconds != 0
							&& (curTime - forgetCarrierDataInSeconds > ini.get_value_int(0))) {
							doLoad = false;
						}
						else {
							ci.lastCarrierLogin = ini.get_value_int(0);
						}
					}
					else if (ini.is_value("docked"))
					{
						wstring& dockedShipName = stows(ini.get_value_string(0));
						const auto& accData = HkGetAccountByCharname(dockedShipName);
						// skip loading renamed/deleted docked ships
						if(!accData){
							continue;
						}

						nameToDockedInfoMap[dockedShipName].carrierName = carrierName.c_str();
						nameToDockedInfoMap[dockedShipName].lastDockedSolar = CreateID(ini.get_value_string(1));

						ci.dockedShipList.push_back(dockedShipName);
						dockedCount++;
					}
				}
				if (doLoad) {
					nameToCarrierInfoMap[carrierName] = ci;
					carrierCount++;
				}
			}
		}
		ini.close();
	}
	ConPrint(L"DockingModules: Loaded %u equipment\n", dockingModAmount);
	ConPrint(L"DockingModules: Found %u ships docked on %u carriers\n", dockedCount, carrierCount);
	ConPrint(L"DockingModules: Allowing ships below the cargo capacity of %i to dock\n", cargoCapacityLimit);
}

void SaveData()
{
	if (nameToCarrierInfoMap.empty()){
		return;
	}
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);
	string path = string(datapath) + "\\Accts\\MultiPlayer\\docking_module\\mobile_docking.ini";
	FILE *file = fopen(path.c_str(), "w");
	if (file)
	{
		for (const auto& ci : nameToCarrierInfoMap)
		{
			if (ci.second.dockedShipList.empty()) {
				continue;
			}
			fprintf(file, "[CarrierData]\n");
			fprintf(file, "carrier = %ls\n", ci.first.c_str());
			fprintf(file, "lastLogin = %u\n", (uint)ci.second.lastCarrierLogin);
			for (const wstring& dockedName : ci.second.dockedShipList)
			{
				const auto& lastSolarInfo = Universe::get_base(nameToDockedInfoMap[dockedName.c_str()].lastDockedSolar);
				fprintf(file, "docked = %ls, %s\n", dockedName.c_str(), lastSolarInfo->cNickname);
			}
		}

		fclose(file);
	}
}

void HkTimerCheckKick()
{
	returncode = DEFAULT_RETURNCODE;

	if (time(0) % saveFrequency == 0)
	{
		SaveData();
	}
}

bool RemoveShipFromLists(const wstring& dockedShipName, boolean isForced)
{
	if (!nameToDockedInfoMap.count(dockedShipName)) {
		return false;
	}
	uint dockedClientID = HkGetClientIdFromCharname(dockedShipName);

	if (dockedClientID != -1)
	{
		if (isForced) 
		{
			const auto& dockedInfo = idToDockedInfoMap[dockedClientID];
			const auto& baseInfo = Universe::get_base(dockedInfo->lastDockedSolar);
			const auto& sysInfo = Universe::get_system(baseInfo->iSystemID);
			wstring& baseName = HkGetWStringFromIDS(baseInfo->iBaseIDS);
			wstring& sysName = HkGetWStringFromIDS(sysInfo->strid_name);
			PrintUserCmdText(dockedClientID, L"Carrier has jettisonned your ship. Returning to %ls, %ls.", baseName.c_str(), sysName.c_str());
		}
		idToDockedInfoMap.erase(dockedClientID);
	}

	wstring& carrierName = nameToDockedInfoMap[dockedShipName].carrierName;

	if (nameToCarrierInfoMap.count(carrierName))
	{
		auto& dockedList = nameToCarrierInfoMap[carrierName].dockedShipList;
		for (auto& iter = dockedList.begin(); iter != dockedList.end(); iter++) {
			if (*iter == dockedShipName) {
				dockedList.erase(iter);
				break;
			}
		}
	}
	nameToDockedInfoMap.erase(dockedShipName);

	return true;
}

// Returns count of installed docking modules on ship of specific client.
uint GetInstalledModules(uint iClientID)
{
	uint modules = 0;

	// Check to see if the vessel undocking currently has a docking module equipped
	for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
	{
		if (find(dockingModuleEquipmentIds.begin(), dockingModuleEquipmentIds.end(), item->iArchID) != dockingModuleEquipmentIds.end())
		{
			if (item->bMounted)
			{
				modules++;
			}
		}
	}

	return modules;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall BaseExit(uint iBaseID, uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	mobiledockClients[iClientID].iDockingModulesInstalled = GetInstalledModules(iClientID);
	
	if (idToCarrierInfoMap.count(iClientID)) {
		// Normalize the docking modules available, with the number of people currently docked
		mobiledockClients[iClientID].iDockingModulesAvailable = (mobiledockClients[iClientID].iDockingModulesInstalled - idToCarrierInfoMap[iClientID]->dockedShipList.size());
	}
	else {
		mobiledockClients[iClientID].iDockingModulesAvailable = mobiledockClients[iClientID].iDockingModulesInstalled;
	}

}

void __stdcall PlayerLaunch_AFTER(unsigned int ship, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;

	// If not docked on another ship, skip processing.
	if (!idToDockedInfoMap.count(client))
	{
		return;
	}

	uint carrierClientID = HkGetClientIdFromCharname(idToDockedInfoMap[client]->carrierName.c_str());

	// If carrier is present at server - do it, if not - whatever. Plugin erases all associated client data after disconnect. 
	if (carrierClientID != -1)
	{
		uint carrierShipID;
		pub::Player::GetShip(carrierClientID, carrierShipID);
		if (carrierShipID == 0) {
			//carrier docked somewhere, undock into that base.
			uint baseID;
			pub::Player::GetBase(carrierClientID, baseID);
			pub::Player::ForceLand(client, baseID);
		}
		else {
			//teleport the player using same method as jumpdrives
			CUSTOM_JUMP_CALLOUT_STRUCT jumpData;
			Vector pos;
			Matrix ori;
			uint iSystemID;

			pub::SpaceObj::GetLocation(carrierShipID, pos, ori);
			pub::SpaceObj::GetSystem(carrierShipID, iSystemID);

			pos.x += ori.data[0][1] * set_iMobileDockOffset;
			pos.y += ori.data[1][1] * set_iMobileDockOffset;
			pos.z += ori.data[2][1] * set_iMobileDockOffset;

			jumpData.iClientID = client;
			jumpData.iSystemID = iSystemID;
			jumpData.pos = pos;
			jumpData.ori = ori;
			jumpData.jumpType = 2; // to avoid producing jump VFX

			Plugin_Communication(PLUGIN_MESSAGE::CUSTOM_JUMP_CALLOUT, &jumpData);

			mobiledockClients[carrierClientID].iDockingModulesAvailable++;
		}
	}
	else
	{
		pub::Player::ForceLand(client, idToDockedInfoMap[client]->lastDockedSolar);
	}

	wstring dockedCharname = (const wchar_t*)Players.GetActiveCharacterName(client);
	RemoveShipFromLists(dockedCharname, false);
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
		if (HkDistance3DByShip(iShip, iTargetShip) > mobileDockingRange)
		{
			PrintUserCmdText(client, L"Ship is out of range");
			return 0;
		}

		const uint iTargetClientID = HkGetClientIDByShip(iTargetShip);
		// Check that the target ship has an empty docking module.
		if (!iTargetClientID || mobiledockClients[iTargetClientID].iDockingModulesAvailable == 0)
		{
			PrintUserCmdText(client, L"Target ship has no free docking capacity");
			return 0;
		}

		// Check that the requesting ship is of the appropriate size to dock.
		const auto& shipInfo = Archetype::GetShip(Players[client].iShipArchetype);
		if (shipInfo->fHoldSize > cargoCapacityLimit)
		{
			PrintUserCmdText(client, L"Target ship cannot dock a ship of your size.");
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

void __stdcall BaseEnter(uint iBaseID, uint client)
{
	returncode = DEFAULT_RETURNCODE;

	if (idToDockedInfoMap.count(client))
	{
		uint CarrierID = HkGetClientIdFromCharname(idToDockedInfoMap[client]->carrierName);
		// Clear the market. We don't support transfers in this version.
		SendResetMarketOverride(client);

		// Set the base name
		wstring status = L"<RDL><PUSH/>";
		status += L"<TEXT>Currently docked on: " + XMLText(idToDockedInfoMap[client]->carrierName);
		if (CarrierID != -1) {
			// if carrier online, add system info.
			const auto& sysInfo = Universe::get_system(Players[CarrierID].iSystemID);
			wstring& systemName = HkGetWStringFromIDS(sysInfo->strid_name);
			status += L", " + systemName;
		}
		status += L"</TEXT><PARA/><PARA/>";
		status += L"<POP/></RDL>";
		SendSetBaseInfoText2(client, status);
	}

}

bool UserCmd_Process(uint client, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	if (wscCmd.find(L"/listdocked") == 0)
	{
		if (!idToCarrierInfoMap.count(client) || idToCarrierInfoMap[client]->dockedShipList.empty())
		{
			PrintUserCmdText(client, L"No ships currently docked");
			return true;
		}
		
		uint counter = 0;
		PrintUserCmdText(client, L"Docked ships:");
		for (wstring& dockedShip : idToCarrierInfoMap[client]->dockedShipList)
		{
			counter++;
			uint dockedClientID = HkGetClientIdFromCharname(dockedShip.c_str());
			if (dockedClientID != -1) {
				const auto& shipInfo = Archetype::GetShip(Players[dockedClientID].iShipArchetype);
				wstring& shipName = HkGetWStringFromIDS(shipInfo->iIdsName);
				PrintUserCmdText(client, L"%u. %ls - %ls - ONLINE", counter, dockedShip.c_str(), shipName.c_str());
			}
			else
			{
				PrintUserCmdText(client, L"%u. %ls - OFFLINE", counter, dockedShip.c_str());
			}
			counter++;
		}
		return true;
	}
	else if (wscCmd.find(L"/conn") == 0 || wscCmd.find(L"/return") == 0)
	{
		// This plugin always runs before the Conn Plugin runs it's /conn function. Verify that there are no docked ships.
		if (mobiledockClients[client].iDockingModulesAvailable != mobiledockClients[client].iDockingModulesInstalled)
		{
			PrintUserCmdText(client, L"You cannot use this command if you have vessels docked with you!");
			returncode = SKIPPLUGINS;
			return true;
		}
	}
	else if (wscCmd.find(L"/jettisonship") == 0)
	{
		if (!idToCarrierInfoMap.count(client) || idToCarrierInfoMap[client]->dockedShipList.empty())
		{
			PrintUserCmdText(client, L"No ships currently docked");
		}
		// Get the supposed ship we should be ejecting from the command parameters
		wstring& selectedShip = Trim(GetParam(wscCmd, ' ', 1));
		if (selectedShip.empty())
		{
			PrintUserCmdText(client, L"Usage: /jettisonship <charName/charNr>");
			return true;
		}
		uint charNumber = ToInt(selectedShip);
		const auto& dockedShipList = idToCarrierInfoMap[client]->dockedShipList;

		if (charNumber > 0 && charNumber <= dockedShipList.size()) {
			selectedShip = dockedShipList.at(charNumber - 1);
		}
		else 
		{
			PrintUserCmdText(client, L"ERR Invalid docked ship index");
			return true;
		}

		if (!RemoveShipFromLists(selectedShip, true))
		{
			PrintUserCmdText(client, L"ERR this ship is not onboard!");
		}
		else
		{
			PrintUserCmdText(client, L"%ls jettisoned", selectedShip.c_str());
		}
		return true;

	}
	else if (wscCmd.find(L"/allowdock") == 0)
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
		const uint iTargetClientID = HkGetClientIDByShip(iTargetShip);
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
		if (mobiledockClients[client].iDockingModulesAvailable <= 0)
		{
			mapPendingDockingRequests.erase(iTargetClientID);
			PrintUserCmdText(client, L"No free docking modules available.");
			return true;
		}

		// The client is free to dock, erase from the pending list and handle
		mapPendingDockingRequests.erase(iTargetClientID);

		string scProxyBase = HkGetPlayerSystemS(client) + "_proxy_base";
		uint iBaseID;
		if (pub::GetBaseID(iBaseID, scProxyBase.c_str()) == -4)
		{
			PrintUserCmdText(client, L"No proxy base in system detected. Contact a developer about this please. Required base: %s", scProxyBase);
			return true;
		}

		// Save the carrier info
		const wchar_t* carrierName = (const wchar_t*)Players.GetActiveCharacterName(client);
		const wchar_t* dockedName = (const wchar_t*)Players.GetActiveCharacterName(iTargetClientID);

		nameToDockedInfoMap[dockedName].carrierName = carrierName;
		nameToDockedInfoMap[dockedName].lastDockedSolar = Players[iTargetClientID].iLastBaseID;
		nameToCarrierInfoMap[carrierName].dockedShipList.push_back(dockedName);
		nameToCarrierInfoMap[carrierName].lastCarrierLogin = time(0);
		idToCarrierInfoMap[client] = &nameToCarrierInfoMap[carrierName];
		idToDockedInfoMap[iTargetClientID] = &nameToDockedInfoMap[dockedName];

		mobiledockClients[client].iDockingModulesAvailable--;

		// Land the ship on the proxy base
		pub::Player::ForceLand(iTargetClientID, iBaseID);
		PrintUserCmdText(client, L"Ship docked");

		return true;
	}
	return false;
}

void __stdcall DisConnect(unsigned int iClientID, enum  EFLConnection state)
{
	if (idToCarrierInfoMap.count(iClientID)) {
		for (const auto& dockedPlayer : idToCarrierInfoMap[iClientID]->dockedShipList)
		{
			uint dockedClientID = HkGetClientIdFromCharname(dockedPlayer.c_str());
			if(dockedClientID != -1)
			{
				const auto& dockedInfo = idToDockedInfoMap[dockedClientID];
				const auto& baseInfo = Universe::get_base(dockedInfo->lastDockedSolar);
				const auto& sysInfo = Universe::get_system(baseInfo->iSystemID);
				wstring& baseName = HkGetWStringFromIDS(baseInfo->iBaseIDS);
				wstring& sysName = HkGetWStringFromIDS(sysInfo->strid_name);
				PrintUserCmdText(dockedClientID, L"Carrier logged off, redirecting undock to %ls, %ls", baseName.c_str(), sysName.c_str());
			}
		}
	}
	mobiledockClients.erase(iClientID);
	mapPendingDockingRequests.erase(iClientID);
	idToCarrierInfoMap.erase(iClientID);
	idToDockedInfoMap.erase(iClientID);
}

void __stdcall CharacterSelect_AFTER(struct CHARACTER_ID const & cId, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	// Erase all plugin info associated with the client in case if the person has switched characters to prevent any bugs.
	mobiledockClients.erase(iClientID);
	mapPendingDockingRequests.erase(iClientID);
	idToCarrierInfoMap.erase(iClientID);
	idToDockedInfoMap.erase(iClientID);

	// Update count of installed modules in case if client left his ship in open space before.
	mobiledockClients[iClientID].iDockingModulesAvailable = mobiledockClients[iClientID].iDockingModulesInstalled = GetInstalledModules(iClientID);
	
	const wchar_t* charname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
	if (nameToCarrierInfoMap.count(charname)) {
		idToCarrierInfoMap[iClientID] = &nameToCarrierInfoMap[charname];
		for (const auto& dockedShipName : idToCarrierInfoMap[iClientID]->dockedShipList) {
			uint dockedClientID = HkGetClientIdFromCharname(dockedShipName.c_str());
			if (dockedClientID != -1)
			{
				PrintUserCmdText(dockedClientID, L"Carrier ship has come online.");
			}
		}
	}
	else if (nameToDockedInfoMap.count(charname)) {
		idToDockedInfoMap[iClientID] = &nameToDockedInfoMap[charname];
		uint carrierClientID = HkGetClientIdFromCharname(idToDockedInfoMap[iClientID]->carrierName.c_str());
		if (carrierClientID != -1)
		{
			const auto& shipInfo = Archetype::GetShip(Players[iClientID].iShipArchetype);
			wstring& shipName = HkGetWStringFromIDS(shipInfo->iIdsName);
			PrintUserCmdText(carrierClientID, L"INFO: Docked %ls, %ls has come online", shipName.c_str(), charname);

			const auto& carrierInfo = Players[carrierClientID];
			const auto& sysInfo = Universe::get_system(carrierInfo.iSystemID);
			wstring& sysName = HkGetWStringFromIDS(sysInfo->strid_name);
			if (carrierInfo.iShipID)
			{
				Vector pos;
				Matrix ori;
				pub::SpaceObj::GetLocation(carrierInfo.iShipID, pos, ori);
				wstring& sector = VectorToSectorCoord(sysInfo->id, pos);
				PrintUserCmdText(iClientID, L"Carrier already in flight, %ls %ls", sysName.c_str(), sector.c_str());
			}
			else if(carrierInfo.iBaseID)
			{
				const auto& baseInfo = Universe::get_base(carrierInfo.iBaseID);
				wstring& baseName = HkGetWStringFromIDS(baseInfo->iBaseIDS);
				PrintUserCmdText(iClientID, L"Carrier currently docked on %ls, %ls", baseName.c_str(), sysName.c_str());
			}
			else
			{
				PrintUserCmdText(iClientID, L"Carrier online in %ls system. Ready to launch.", sysName.c_str());
			}
		}
		else
		{
			const auto& dockedInfo = idToDockedInfoMap[iClientID];
			const auto& baseInfo = Universe::get_base(dockedInfo->lastDockedSolar);
			const auto& sysInfo = Universe::get_system(baseInfo->iSystemID);
			wstring& baseName = HkGetWStringFromIDS(baseInfo->iBaseIDS);
			wstring& sysName = HkGetWStringFromIDS(sysInfo->strid_name);

			PrintUserCmdText(iClientID, L"Carrier currently offline. Last docked base: %ls, %ls", baseName.c_str(), sysName.c_str());
		}
	}
}

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
		HkUnloadStringDLLs();
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT PLUGIN_RETURNCODE Get_PluginReturnCode()
{
	return returncode;
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
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch_AFTER, PLUGIN_HkIServerImpl_PlayerLaunch_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterSelect_AFTER, PLUGIN_HkIServerImpl_CharacterSelect_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&DisConnect, PLUGIN_HkIServerImpl_DisConnect, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkTimerCheckKick, PLUGIN_HkTimerCheckKick, 0));

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 3));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Dock_Call, PLUGIN_HkCb_Dock_Call, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseEnter, PLUGIN_HkIServerImpl_BaseEnter, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseExit, PLUGIN_HkIServerImpl_BaseExit, 0));

	return p_PI;
}
