/**
 * Mobile Docking Plugin for FLHook
 * Initial Version by Cannon
 * Using some work written by Alley
 * Rework by Remnant
 */

#include "Main.h"
#include <ctime>

PLUGIN_RETURNCODE returncode;
map<uint, CLIENT_DATA> mobiledockClients;
map<uint, uint> mapPendingDockingRequests;
vector<uint> dockingModuleEquipmentIds;

// Above how much cargo capacity, should a ship be rejected as a docking user?
int cargoCapacityLimit = 275;

// How much time will player be given before kick if carrier wants to jettison him.
int jettisonKickTime = 15;

// Delayed actions, which need to be done. Look at HkTimerCheckKick().
vector<ActionJettison> jettisonList;

void HkTimerCheckKick()
{
	returncode = DEFAULT_RETURNCODE;

	for (vector<ActionJettison>::iterator it = jettisonList.begin(); it != jettisonList.end(); )
	{
		it->timeLeft--;
		if (it->timeLeft == 0)
		{
			uint checkCarrierClientID = HkGetClientIdFromCharname(it->carrierCharname);
			uint checkDockedClientID = HkGetClientIdFromCharname(it->dockedCharname);

			// If both carrier and docked ship are still at server.
			if (checkDockedClientID != -1 && checkCarrierClientID != -1)
			{
				// If the client is still docked
				if (mobiledockClients[checkDockedClientID].wscDockedWithCharname == it->carrierCharname)
				{
					HkKickReason(ARG_CLIENTID(checkDockedClientID), L"Forced undocking.");

					// If carrier died, we don't need this message.
					if (!mobiledockClients[checkDockedClientID].carrierDied)
						PrintUserCmdText(checkCarrierClientID, L"Jettisoning ship...");
				}
			}

			it = jettisonList.erase(it);
		}
		else
		{
			it++;
		}
	}
}

// Easy creation of delayed action.
void DelayedJettison(int delayTimeSecond, wstring carrierCharname, wstring dockedCharname)
{
	ActionJettison action;
	action.timeLeft = delayTimeSecond;
	action.carrierCharname = carrierCharname;
	action.dockedCharname = dockedCharname;
	jettisonList.push_back(action);
}

void JettisonShip(uint carrierClientID, uint dockedClientID)
{
	const wchar_t* carrierCharname = (const wchar_t*)Players.GetActiveCharacterName(carrierClientID);
	const wchar_t* dockedCharname = (const wchar_t*)Players.GetActiveCharacterName(dockedClientID);

	if (dockedClientID != -1)
	{
		if (!mobiledockClients[dockedClientID].carrierDied)
		{
			PrintUserCmdText(carrierClientID, L"Ship warned. If it doesn't undock in %i seconds, it will be kicked by force.", jettisonKickTime);
			PrintUserCmdText(dockedClientID, L"Carrier wants to jettison your ship. Undock willingly or you will be kicked after %i seconds.", jettisonKickTime);
		}
		else
		{
			PrintUserCmdText(dockedClientID, L"ALERT! Carrier ship is being destroyed. Launch ship or you will die in explosion.");
		}
		pub::Audio::PlaySoundEffect(dockedClientID, CreateID("rtc_klaxon_loop"));

		// Create delayed action.
		DelayedJettison(jettisonKickTime, carrierCharname, dockedCharname);
	}
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

void UpdateCarrierLocationInformation(uint dockedClientId, uint carrierShip)
{

	// Prepare references to the docked ship's copy of the carrierShip's position and rotation for manipulation
	Vector& carrierPos = mobiledockClients[dockedClientId].carrierPos;
	Matrix& carrierRot = mobiledockClients[dockedClientId].carrierRot;


	// If the carrier is out in space, simply set the undock location to where the carrier is currently
	pub::SpaceObj::GetSystem(carrierShip, mobiledockClients[dockedClientId].carrierSystem);
	pub::SpaceObj::GetLocation(carrierShip, carrierPos, carrierRot);

	carrierPos.x += carrierRot.data[0][1] * set_iMobileDockOffset;
	carrierPos.y += carrierRot.data[1][1] * set_iMobileDockOffset;
	carrierPos.z += carrierRot.data[2][1] * set_iMobileDockOffset;
}

// Overloaded function used with a specific carrier location, instead of extracting it from the ship itself
void UpdateCarrierLocationInformation(uint dockedClientId, Vector pos, Matrix rot)
{
	// Prepare references to the docked ship's copy of the carrierShip's position and rotation for manipulation
	Vector& carrierPos = mobiledockClients[dockedClientId].carrierPos;
	Matrix& carrierRot = mobiledockClients[dockedClientId].carrierRot;

	carrierPos = pos;
	carrierRot = rot;

	carrierPos.x += carrierRot.data[0][1] * set_iMobileDockOffset;
	carrierPos.y += carrierRot.data[1][1] * set_iMobileDockOffset;
	carrierPos.z += carrierRot.data[2][1] * set_iMobileDockOffset;
}

inline void UndockShip(uint iClientID)
{
	// If the ship was docked to someone, erase it from docked ship list.
	if (mobiledockClients[iClientID].mobileDocked)
	{
		uint carrierClientID = HkGetClientIdFromCharname(mobiledockClients[iClientID].wscDockedWithCharname);

		// If carrier is present at server - do it, if not - whatever. Plugin erases all associated client data after disconnect. 
		if (carrierClientID != -1 && !mobiledockClients[iClientID].carrierDied)
		{
			wstring charname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
			mobiledockClients[carrierClientID].mapDockedShips.erase(charname);
			mobiledockClients[carrierClientID].iDockingModulesAvailable++;
		}
	}

	mobiledockClients.erase(iClientID);
	mapPendingDockingRequests.erase(iClientID);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Load the configuration
void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	// The path to the data file.
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	// Create the directory if it doesn't exist
	string moddir = string(datapath) + R"(\Accts\MultiPlayer\docking_module\)";
	CreateDirectoryA(moddir.c_str(), 0);

	// Plugin configuration
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\dockingmodules.cfg";

	int dockingModAmount = 0;
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
				}
			}
		}
		ini.close();
	}

	ConPrint(L"DockingModules: Loaded %u equipment\n", dockingModAmount);
	ConPrint(L"DockingModules: Allowing ships below the cargo capacity of %i to dock\n", cargoCapacityLimit);
}

void __stdcall BaseExit(uint iBaseID, uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	mobiledockClients[iClientID].iDockingModulesInstalled = GetInstalledModules(iClientID);

	// Normalize the docking modules available, with the number of people currently docked
	mobiledockClients[iClientID].iDockingModulesAvailable = (mobiledockClients[iClientID].iDockingModulesInstalled - mobiledockClients[iClientID].mapDockedShips.size());

	// If this is a ship which is currently docked, clear the market
	if (mobiledockClients[iClientID].mobileDocked)
	{
		SendResetMarketOverride(iClientID);
	}

}

// Temporary storage for client data to be handled in LaunchPosHook.
CLIENT_DATA undockingShip;

void __stdcall PlayerLaunch(unsigned int iShip, unsigned int client)
{

	returncode = DEFAULT_RETURNCODE;

	uint carrier_client = HkGetClientIdFromCharname(mobiledockClients[client].wscDockedWithCharname);

	wstring clientName = (const wchar_t*)Players.GetActiveCharacterName(client);

	if (mobiledockClients[client].mobileDocked)
	{

		returncode = SKIPPLUGINS;

		// Set last base to last real base this ship was on. POB support will be added in 0.9.X version.
		Players[client].iLastBaseID = mobiledockClients[client].iLastBaseID;

		// Check if carrier died.
		if (mobiledockClients[client].carrierDied)
		{
			// If died carrier and docked ship are in same system.
			if (Players[client].iSystemID == mobiledockClients[client].carrierSystem)
			{
				// Now that the data is prepared, send the player to the carrier's location
				undockingShip = mobiledockClients[client];

				// Clear the client out of the mobiledockClients now that it's no longer docked
				mobiledockClients.erase(client);
			}
			// If not - redirect to proxy base in carrier's system.
			else
			{
				wstring scProxyBase = HkGetSystemNickByID(mobiledockClients[client].carrierSystem) + L"_proxy_base";
				if (pub::GetBaseID(mobiledockClients[client].proxyBaseID, (wstos(scProxyBase)).c_str()) == -4)
				{
					PrintUserCmdText(client, L"No proxy base in system detected. Contact a developer about this please. Required base: %ws", scProxyBase);
					return;
				}

				mobiledockClients[client].carrierSystem = -1;
			}
			return;
		}
		else if (carrier_client == -1)
		{
			// If the carrier doesn't exist for some reason, force the client to dock on it's last known base
			uint iBaseID = mobiledockClients[client].iLastBaseID;
			mobiledockClients[client].undockBase = Universe::get_base(iBaseID);
			mobiledockClients[client].baseUndock = true;

			return;

		}

		//Get the carrier ship information
		uint carrierShip;
		pub::Player::GetShip(carrier_client, carrierShip);

		uint clientShip;
		pub::Player::GetShip(client, clientShip);

		// Check to see if the carrier is currently in a base. If so, force the client to dock on that base.
		if (!carrierShip)
		{

			uint iBaseID;
			pub::Player::GetBase(carrier_client, iBaseID);

			if (iBaseID)
			{
				// Set the flags which the PlayerLaunch_AFTER uses to handle teleporting to the carriers base
				mobiledockClients[client].undockBase = Universe::get_base(iBaseID);
				mobiledockClients[carrier_client].mapDockedShips.erase(clientName);
				mobiledockClients[client].baseUndock = true;
				return;
			}
		}

		// If carrier and docked ship are in same system.
		if (Players[client].iSystemID == Players[carrier_client].iSystemID)
		{
			// Update the internal values of the docked ship pretaining to the carrier
			UpdateCarrierLocationInformation(client, carrierShip);

			// Now that the data is prepared, send the player to the carrier's location
			undockingShip = mobiledockClients[client];

			// Clear the client out of the mobiledockClients now that it's no longer docked
			mobiledockClients.erase(client);

			// Remove charname from carrier's mapDockedShips.
			mobiledockClients[carrier_client].mapDockedShips.erase(clientName);
			mobiledockClients[carrier_client].iDockingModulesAvailable++;
		}
		// If not - redirect to proxy base in carrier's system.
		else
		{
			mobiledockClients[client].carrierSystem = -1;
		}
	}
}

bool __stdcall LaunchPosHook(uint space_obj, struct CEqObj &p1, Vector &pos, Matrix &rot, int dock_mode)
{
	returncode = DEFAULT_RETURNCODE;

	// Redirect the ship to carrier's position. Can bug with POB plugin, changes may be required.
	if (undockingShip.proxyBaseID == space_obj)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		rot = undockingShip.carrierRot;
		pos = undockingShip.carrierPos;
		undockingShip.proxyBaseID = 0;
	}
	return true;
}

void __stdcall PlayerLaunch_AFTER(unsigned int ship, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;

	// Is the client flagged to dock at a base after exiting?
	if (mobiledockClients[client].baseUndock)
	{
		uint systemID = Players[client].iSystemID;

		pub::Player::ForceLand(client, mobiledockClients[client].undockBase->iBaseID);

		if (mobiledockClients[client].undockBase->iSystemID != systemID)
		{
			// Update current system stat in player list to be displayed relevantly.
			Server.BaseEnter(mobiledockClients[client].undockBase->iBaseID, client);
			Server.BaseExit(mobiledockClients[client].undockBase->iBaseID, client);
			wstring wscCharFileName;
			HkGetCharFileName((const wchar_t*)Players.GetActiveCharacterName(client), wscCharFileName);
			wscCharFileName += L".fl";
			CHARACTER_ID cID;
			strcpy(cID.szCharFilename, wstos(wscCharFileName.substr(0, 14)).c_str());
			Server.CharacterSelect(cID, client);
		}

		mobiledockClients.erase(client);
	}

	// Land ship to proxy base in carrier's system if they are in different systems.
	if (mobiledockClients[client].carrierSystem == -1)
	{
		if (!mobiledockClients[client].carrierDied)
		{
			string scProxyBase = HkGetPlayerSystemS(HkGetClientIdFromCharname(mobiledockClients[client].wscDockedWithCharname)) + "_proxy_base";
			if (pub::GetBaseID(mobiledockClients[client].proxyBaseID, scProxyBase.c_str()) == -4)
			{
				PrintUserCmdText(client, L"No proxy base in system detected. Contact a developer about this please. Required base: %s", scProxyBase);
				return;
			}
		}

		pub::Player::ForceLand(client, mobiledockClients[client].proxyBaseID);

		// Send the message because if carrier goes to another system, docked ships remain in previous with outdated system navmap. We notify client about it is being updated.
		PrintUserCmdText(client, L"Navmap updated successfully.");

		// Update current system stat in player list to be displayed relevantly.
		Server.BaseEnter(mobiledockClients[client].proxyBaseID, client);
		Server.BaseExit(mobiledockClients[client].proxyBaseID, client);
		wstring wscCharFileName;
		HkGetCharFileName((const wchar_t*)Players.GetActiveCharacterName(client), wscCharFileName);
		wscCharFileName += L".fl";
		CHARACTER_ID cID;
		strcpy(cID.szCharFilename, wstos(wscCharFileName.substr(0, 14)).c_str());
		Server.CharacterSelect(cID, client);

		// Update current system stat in plugin data.
		Universe::IBase* base = Universe::get_base(mobiledockClients[client].proxyBaseID);
		mobiledockClients[client].carrierSystem = base->iSystemID;
	}
}

// If this is a docking request at a player ship then process it.
int __cdecl Dock_Call(unsigned int const &iShip, unsigned int const &iBaseID, int& iCancel, enum DOCK_HOST_RESPONSE& response)
{
	returncode = DEFAULT_RETURNCODE;

	//if not a player dock, skip
	uint client = HkGetClientIDByShip(iShip);
	if (client && response == DOCK && iCancel == -1)
	{
		// If no target then ignore the request.
		uint iTargetShip;
		pub::SpaceObj::GetTarget(iShip, iTargetShip);
		if (!iTargetShip)
			return 0;

		uint iType;
		pub::SpaceObj::GetType(iTargetShip, iType);
		if (!(iType & (OBJ_FREIGHTER | OBJ_TRANSPORT | OBJ_GUNBOAT | OBJ_CRUISER | OBJ_CAPITAL)))
			return 0;

		// If target is not player ship or ship is too far away then ignore the request.
		const uint iTargetClientID = HkGetClientIDByShip(iTargetShip);
		if (!iTargetClientID || HkDistance3DByShip(iShip, iTargetShip) > 1000.0f)
		{
			PrintUserCmdText(client, L"Ship is out of range");
			iCancel = -1;
			response = DOCK_DENIED;
			return 0;
		}

		// Check that the target ship has an empty docking module.
		if (mobiledockClients[iTargetClientID].iDockingModulesAvailable == 0)
		{
			PrintUserCmdText(client, L"Target ship has no free docking capacity");
			iCancel = -1;
			response = DOCK_DENIED;
			return 0;
		}

		// Check that the requesting ship is of the appropriate size to dock.
		CShip* cship = dynamic_cast<CShip*>(HkGetEqObjFromObjRW(reinterpret_cast<IObjRW*>(HkGetInspect(client))));
		if (cship->shiparch()->fHoldSize > cargoCapacityLimit)
		{
			PrintUserCmdText(client, L"Target ship cannot dock a ship of your size.");
			iCancel = -1;
			response = DOCK_DENIED;
			return 0;
		}

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

	if (mobiledockClients[client].mobileDocked)
	{
		// Clear the market. We don't support transfers in this version.
		SendResetMarketOverride(client);

		// Set the base name
		wstring status = L"<RDL><PUSH/>";
		status += L"<TEXT>" + XMLText(mobiledockClients[client].wscDockedWithCharname) + L"</TEXT><PARA/><PARA/>";
		status += L"<POP/></RDL>";
		SendSetBaseInfoText2(client, status);
	}

}

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
			if (!mobiledockClients[client].mapDockedShips.empty())
			{
				// Send a system switch to force each ship to launch
				for (map<wstring, wstring>::iterator i = mobiledockClients[client].mapDockedShips.begin();
					i != mobiledockClients[client].mapDockedShips.end(); ++i)
				{
					uint iDockedClientID = HkGetClientIdFromCharname(i->second);

					// Update the coordinates the given ship should launch to.
					UpdateCarrierLocationInformation(iDockedClientID, cship->get_position(), cship->get_orientation());

					// Carrier is no more. Set the flag.
					mobiledockClients[iDockedClientID].carrierDied = true;

					// Due to the carrier not existing anymore, we have to pull the system information from the carriers historical location.
					mobiledockClients[iDockedClientID].carrierSystem = cship->iSystem;

					JettisonShip(client, iDockedClientID);
				}

				// Clear the carrier from the list
				mobiledockClients[client].mapDockedShips.clear();
				mobiledockClients[client].iDockingModulesAvailable = mobiledockClients[client].iDockingModulesInstalled;
			}
		}
	}
}


bool UserCmd_Process(uint client, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	if (wscCmd.find(L"/listdocked") == 0)
	{
		if (mobiledockClients[client].mapDockedShips.empty())
		{
			PrintUserCmdText(client, L"No ships currently docked");
		}
		else
		{
			PrintUserCmdText(client, L"Docked ships:");
			for (map<wstring, wstring>::iterator i = mobiledockClients[client].mapDockedShips.begin();
				i != mobiledockClients[client].mapDockedShips.end(); ++i)
			{
				PrintUserCmdText(client, i->first);
			}
		}
		return true;
	}
	else if (wscCmd.find(L"/conn") == 0 || wscCmd.find(L"/return") == 0)
	{
		// This plugin always runs before the Conn Plugin runs it's /conn function. Verify that there are no docked ships.
		if (!mobiledockClients[client].mapDockedShips.empty())
		{
			PrintUserCmdText(client, L"You cannot use this command if you have vessels docked with you!");
			returncode = SKIPPLUGINS;
			return true;
		}
	}
	else if (wscCmd.find(L"/jettisonship") == 0)
	{
		// Get the supposed ship we should be ejecting from the command parameters
		wstring charname = Trim(GetParam(wscCmd, ' ', 1));
		if (charname.empty())
		{
			PrintUserCmdText(client, L"Usage: /jettisonship <charname>");
			return true;
		}

		// Only allow jettisonning a ship if the carrier is undocked
		uint carrierShip;
		pub::Player::GetShip(client, carrierShip);
		if (!carrierShip)
		{
			PrintUserCmdText(client, L"You can only jettison a vessel if you are in space.");
			return true;
		}


		// Check to see if the user listed is actually docked with the carrier at the moment
		if (mobiledockClients[client].mapDockedShips.find(charname) == mobiledockClients[client].mapDockedShips.end())
		{
			PrintUserCmdText(client, L"%s is not docked with you!", charname);
			return true;
		}

		// The player exists. Remove him from the docked list, and kick him into space
		const uint iDockedClientID = HkGetClientIdFromCharname(charname);
		if (iDockedClientID != -1)
		{
			// Update the client with the current carrier location
			UpdateCarrierLocationInformation(iDockedClientID, carrierShip);

			// Force the docked ship to launch. The teleport coordinates have been set by the previous method
			JettisonShip(client, iDockedClientID);
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
		wstring charname = (const wchar_t*)Players.GetActiveCharacterName(iTargetClientID);
		mobiledockClients[client].mapDockedShips[charname] = charname;
		pub::SpaceObj::GetSystem(iShip, mobiledockClients[client].carrierSystem);
		if (mobiledockClients[client].iLastBaseID != 0)
			mobiledockClients[client].iLastBaseID = Players[client].iLastBaseID;

		// Save the docking ship info
		mobiledockClients[iTargetClientID].mobileDocked = true;
		mobiledockClients[iTargetClientID].wscDockedWithCharname = (const wchar_t*)Players.GetActiveCharacterName(client);
		mobiledockClients[iTargetClientID].iLastBaseID = Players[iTargetClientID].iLastBaseID;
		mobiledockClients[iTargetClientID].proxyBaseID = iBaseID;
		pub::SpaceObj::GetSystem(iShip, mobiledockClients[iTargetClientID].carrierSystem);

		mobiledockClients[client].iDockingModulesAvailable--;

		// Land the ship on the proxy base
		pub::Player::ForceLand(iTargetClientID, iBaseID);
		PrintUserCmdText(client, L"Ship docked");

		return true;
	}
	return false;
}

void __stdcall DisConnect(uint iClientID, enum EFLConnection p2)
{
	returncode = DEFAULT_RETURNCODE;

	UndockShip(iClientID);
}

void __stdcall CharacterSelect_AFTER(struct CHARACTER_ID const & cId, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	// Erase all plugin info associated with the client in case if the person has switched characters to prevent any bugs.
	UndockShip(iClientID);

	// Update count of installed modules in case if client left his ship in open space before.
	mobiledockClients[iClientID].iDockingModulesAvailable = mobiledockClients[iClientID].iDockingModulesInstalled = GetInstalledModules(iClientID);
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
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
	}
	return true;
}

#define IS_CMD(a) !wscCmd.compare(L##a)

bool ExecuteCommandString_Callback(CCmds* classptr, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	if (IS_CMD("logactivity"))
	{
		// Log current time in file name.
		std::time_t rawtime;
		std::tm* timeinfo;
		char buffer[80];

		std::time(&rawtime);
		timeinfo = std::localtime(&rawtime);

		std::strftime(buffer, 80, "[%Y-%m-%d]%H-%M-%S", timeinfo);

		string path = "./flhook_logs/logactivity " + string(buffer) + ".log";

		// Lines list to add in new file.
		vector<string> Lines;

		Lines.push_back("DOCK: mobiledockClients | Size = " + to_string(mobiledockClients.size()));

		vector<wstring> DOCKcharnames;
		for (map<uint, CLIENT_DATA>::iterator it = mobiledockClients.begin(); it != mobiledockClients.end(); ++it)
		{
			wstring charname;

			try
			{
				charname = (const wchar_t*)Players.GetActiveCharacterName(it->first);
			}
			catch (...)
			{
				charname = L"<Error>";
			}

			string ID = to_string(it->first);
			string Charname = wstos(charname);
			string Type = it->second.iDockingModulesInstalled == 0 ? "Docked" : "Carrier";
			if (Type == "Carrier")
			{
				wstring docked = L"";
				for (map<wstring, wstring>::iterator cit = it->second.mapDockedShips.begin(); cit != it->second.mapDockedShips.end(); cit++)
				{
					if (docked != L"")
						docked += L" | ";
					docked += cit->first;
				}

				Type += "[" + to_string(it->second.iDockingModulesInstalled) + "](" + wstos(docked) + ")";
			}
			else
			{

				if (it->second.wscDockedWithCharname.empty())
				{
					if (!it->second.mobileDocked)
						continue;
					Type += "[" + wstos(L"<Error>") + "]";
				}
				else
					Type += "[" + wstos(it->second.wscDockedWithCharname) + "]";
			}
			string State = "";

			if (it->first == 0 || it->first > MAX_CLIENT_ID)
				State += "Out of range";

			if (find(DOCKcharnames.begin(), DOCKcharnames.end(), charname) != DOCKcharnames.end())
			{
				if (State != "")
					State += " | ";
				State += "Doubled";
			}

			if (State == "")
				State = "Fine";

			if (charname != L"<Error>")
				DOCKcharnames.push_back(charname);

			Lines.push_back(ID + " " + Charname + " " + Type + " " + State);
		}

		Lines.push_back("");


		vector<string> SERVERlines;

		vector<wstring> SERVERcharnames;
		vector<uint> IDs;
		struct PlayerData *pPD = 0;
		while (pPD = Players.traverse_active(pPD))
		{
			uint clientID;
			try
			{
				clientID = HkGetClientIdFromPD(pPD);
			}
			catch (...)
			{
				clientID = 0;
			}

			string ID;

			if (clientID == 0)
				ID = "<Error>";
			else
				ID = to_string(clientID);

			wstring charname;
			wstring wscIP;

			try
			{
				charname = (const wchar_t*)Players.GetActiveCharacterName(clientID);
			}
			catch (...)
			{
				charname = L"<NotLogged>";
			}

			try
			{
				HkGetPlayerIP(clientID, wscIP);
			}
			catch (...)
			{
				wscIP = L"<Error>";
			}

			string Charname = wstos(charname);
			string State = "";

			if (clientID > MAX_CLIENT_ID)
				State += "Out of range";


			if (find(SERVERcharnames.begin(), SERVERcharnames.end(), charname) != SERVERcharnames.end())
			{
				if (State != "")
					State += " | ";
				State += "DoubledName";
			}

			if (find(IDs.begin(), IDs.end(), clientID) != IDs.end())
			{
				if (State != "")
					State += " | ";
				State += "DoubledID";
			}

			if (State == "")
				State = "Fine";

			if (charname != L"<NotLogged>")
				SERVERcharnames.push_back(charname);
			IDs.push_back(clientID);

			SERVERlines.push_back(ID + " " + Charname + " " + wstos(wscIP) + " " + State);
		}

		Lines.push_back("SERVER: PlayersDB | Size = " + to_string(SERVERlines.size()));
		Lines.insert(Lines.end(), SERVERlines.begin(), SERVERlines.end());

		// Create new file.
		FILE *newfile = fopen(path.c_str(), "w");
		if (newfile)
		{
			for (vector<string>::iterator it = Lines.begin(); it != Lines.end(); ++it)
			{
				fprintf(newfile, (*it + "\n").c_str());
			}

			fclose(newfile);
		}

		ConPrint(L"Saved to: " + stows(path) + L"\n");
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		return true;
	}
	return false;
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
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ExecuteCommandString_Callback, PLUGIN_ExecuteCommandString_Callback, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch_AFTER, PLUGIN_HkIServerImpl_PlayerLaunch_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterSelect_AFTER, PLUGIN_HkIServerImpl_CharacterSelect_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ShipDestroyed, PLUGIN_ShipDestroyed, 0));

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkTimerCheckKick, PLUGIN_HkTimerCheckKick, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 3));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Dock_Call, PLUGIN_HkCb_Dock_Call, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseEnter, PLUGIN_HkIServerImpl_BaseEnter, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseExit, PLUGIN_HkIServerImpl_BaseExit, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch, PLUGIN_HkIServerImpl_PlayerLaunch, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LaunchPosHook, PLUGIN_LaunchPosHook, -1));

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&DisConnect, PLUGIN_HkIServerImpl_DisConnect, 0));

	return p_PI;
}
