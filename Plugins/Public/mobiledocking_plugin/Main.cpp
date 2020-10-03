/**
 * Mobile Docking Plugin for FLHook
 * Initial Version by Cannon
 * Using some work written by Alley
 * Rework by Remnant
 */

#include "Main.h"
#include <ctime>

PLUGIN_RETURNCODE returncode;
map<wstring, CLIENT_DATA> mobiledockClients;
map<uint, uint> mapPendingDockingRequests;
map<uint, list<InviteLink>> mapInviteLinks;
vector<uint> dockingModuleEquipmentIds;

// Above how much cargo capacity, should a ship be rejected as a docking user?
int cargoCapacityLimit = 275;

// How much time will player be given before kick if carrier wants to jettison him.
int jettisonKickTime = 10;

// Delayed actions, which need to be done. Look at HkTimerCheckKick().
vector<ActionJettison> jettisonList;

//Range and local_formal for TRAFFIC CONTROL messages
float set_iLocalChatRange = 9999;
bool set_bLocalTime = false;

//Expiration for offline invitation links
int ExpirationOfflineInvitation = 120;

//Base chosen to have its insides for docking module (excluding cloak/jump/anomaly batts), the choice can be changed by config file.
string shop_inside = "li06_04_base";
wstring GChar(uint client)
{
	wstring wscCharFileName = L"";
	HkGetCharFileName((const wchar_t*)Players.GetActiveCharacterName(client), wscCharFileName);
	return wscCharFileName;
}
wstring GCharByName(wstring name)
{
	wstring wscCharFileName = L"";
	HkGetCharFileName(name, wscCharFileName);
	return wscCharFileName;
}
wstring GCharNameByClient(uint client)
{
	wstring wscCharFileName = L"";
	wscCharFileName = (const wchar_t*)Players.GetActiveCharacterName(client);
	return wscCharFileName;
}
void ImprovedForcedLand(uint client)
{
	// Ask that another plugin handle the beam.
	CUSTOM_BASE_BEAM_STRUCT info;
	info.iClientID = client;
	info.iTargetBaseID = mobiledockClients[GChar(client)].undockPoBID;
	info.bBeamed = false;
	Plugin_Communication(CUSTOM_BASE_BEAM, &info);
	if (!info.bBeamed)
	{
		pub::Player::ForceLand(client, mobiledockClients[GChar(client)].undockNPCBase->iBaseID);
	}
}
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
				auto docked = mobiledockClients.find(GCharByName(it->dockedCharname));
				if (docked != mobiledockClients.end())
				{
					//If carrier inside a base, kill fighter to this base.
					if (mobiledockClients[GChar(checkCarrierClientID)].IsCarrierInBase && docked->second.mobileDocked)
					{
						ForceLaunch(checkDockedClientID);
						ImprovedForcedLand(checkDockedClientID);
						PrintUserCmdText(checkDockedClientID, L"ERR 1 Your access to carrier has been disrupted!");
						PrintUserCmdText(checkCarrierClientID, L"OK 1 Jettisoning ship...");
						return;
					}

					// If carrier died, we don't need it.
					if (!mobiledockClients[GChar(checkDockedClientID)].carrierDied)
					{
						wstring wscCarrierCharFileName = GCharByName(it->carrierCharname);
						wstring wscCharFileName = GCharByName(it->dockedCharname);

						// If the client is still docked
						if (mobiledockClients[wscCharFileName].wscDockedWithCharname == it->carrierCharname)
						{
							//kick to proxy
							mobiledockClients[wscCharFileName].carrierSystem = -1;

							//restore amount of free modules to carrier
							if (mobiledockClients[wscCarrierCharFileName].mapDockedShips.find(it->dockedCharname) != mobiledockClients[wscCarrierCharFileName].mapDockedShips.end())
							{
								mobiledockClients[wscCarrierCharFileName].mapDockedShips.erase(it->dockedCharname);
								mobiledockClients[wscCarrierCharFileName].iDockingModulesAvailable++;
								mobiledockClients.erase(wscCharFileName);
							}

							//ForceLaunch(checkDockedClientID);
							//HkKickReason(ARG_CLIENTID(checkDockedClientID), L"Forced undocking.");
							PrintUserCmdText(checkDockedClientID, L"ERR 2 Your access to carrier has been disrupted!");
							PrintUserCmdText(checkCarrierClientID, L"OK 2 Jettisoning ship...");
						}
						else PrintUserCmdText(checkCarrierClientID, L"ERR 1 Jettisoning process is disrupted, try again.");

					}
					else PrintUserCmdText(checkCarrierClientID, L"ERR 2 Jettisoning process is disrupted, try again.");
				}
				else PrintUserCmdText(checkCarrierClientID, L"ERR 3 Jettisoning process is disrupted, try again.");
			}
			else
			{
				if (checkCarrierClientID != -1)
					PrintUserCmdText(checkCarrierClientID, L"ERR 4 Jettisoning process is disrupted, try again.");
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
		if (!mobiledockClients[GChar(dockedClientID)].carrierDied)
		{
			PrintUserCmdText(carrierClientID, L"Ship warned. If it doesn't undock in %i seconds, it will be kicked by force.", jettisonKickTime);
			PrintUserCmdText(dockedClientID, L"Carrier wants to jettison your ship. Undock willingly or you will be kicked after %i seconds.", jettisonKickTime);
		}
		else
		{
			PrintUserCmdText(dockedClientID, L"ALERT! Carrier ship is being destroyed. Launch ship or you will die in explosion");
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
	wstring DockedClientChar = GChar(dockedClientId);
	// Prepare references to the docked ship's copy of the carrierShip's position and rotation for manipulation
	Vector& carrierPos = mobiledockClients[DockedClientChar].carrierPos;
	Matrix& carrierRot = mobiledockClients[DockedClientChar].carrierRot;


	// If the carrier is out in space, simply set the undock location to where the carrier is currently
	pub::SpaceObj::GetSystem(carrierShip, mobiledockClients[DockedClientChar].carrierSystem);
	pub::SpaceObj::GetLocation(carrierShip, carrierPos, carrierRot);

	carrierPos.x += carrierRot.data[0][1] * set_iMobileDockOffset;
	carrierPos.y += carrierRot.data[1][1] * set_iMobileDockOffset;
	carrierPos.z += carrierRot.data[2][1] * set_iMobileDockOffset;
}

// Overloaded function used with a specific carrier location, instead of extracting it from the ship itself
void UpdateCarrierLocationInformation(uint dockedClientId, Vector pos, Matrix rot)
{
	wstring DockedClientChar = GChar(dockedClientId);
	// Prepare references to the docked ship's copy of the carrierShip's position and rotation for manipulation
	Vector& carrierPos = mobiledockClients[DockedClientChar].carrierPos;
	Matrix& carrierRot = mobiledockClients[DockedClientChar].carrierRot;

	carrierPos = pos;
	carrierRot = rot;

	carrierPos.x += carrierRot.data[0][1] * set_iMobileDockOffset;
	carrierPos.y += carrierRot.data[1][1] * set_iMobileDockOffset;
	carrierPos.z += carrierRot.data[2][1] * set_iMobileDockOffset;
}
inline void UndockShip(uint iClientID)
{
	wstring ClientCharfile = GChar(iClientID);
	// If the ship was docked to someone, erase it from docked ship list.
	if (mobiledockClients[ClientCharfile].mobileDocked)
	{
		uint carrierClientID = HkGetClientIdFromCharname(mobiledockClients[ClientCharfile].wscDockedWithCharname);

		// If carrier is present at server - do it, if not - whatever. Plugin erases all associated client data after disconnect. 
		if (carrierClientID != -1 && !mobiledockClients[ClientCharfile].carrierDied)
		{
			wstring charname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
			wstring CarrierFileName = GChar(carrierClientID);
			mobiledockClients[CarrierFileName].mapDockedShips.erase(charname);
			mobiledockClients[CarrierFileName].iDockingModulesAvailable++;
		}
	}

	mobiledockClients.erase(ClientCharfile);
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
					else if (ini.is_value("local_chat_range"))
					{
						set_iLocalChatRange = ini.get_value_float(0);
					}
					else if (ini.is_value("shop_inside"))
					{
						shop_inside = ini.get_value_string(0);
					}
					else if (ini.is_value("local_time"))
					{
						set_bLocalTime = ini.get_value_bool(0);
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
	wstring ClientCharFile = GChar(iClientID);

	mobiledockClients[ClientCharFile].IsCarrierInBase = false;
	mobiledockClients[ClientCharFile].iDockingModulesInstalled = GetInstalledModules(iClientID);
	// Normalize the docking modules available, with the number of people currently docked
	mobiledockClients[ClientCharFile].iDockingModulesAvailable = (mobiledockClients[ClientCharFile].iDockingModulesInstalled - mobiledockClients[ClientCharFile].mapDockedShips.size());

	// If this is a ship which is currently docked, clear the market
	if (mobiledockClients[ClientCharFile].mobileDocked)
	{
		SendResetMarketOverride(iClientID);
	}

}

// Temporary storage for client data to be handled in LaunchPosHook.
CLIENT_DATA undockingShip;

uint GetCustomBaseForClient(unsigned int client)
{
	// Pass to plugins incase this ship is docked at a custom base.
	CUSTOM_BASE_IS_DOCKED_STRUCT info;
	info.iClientID = client;
	info.iDockedBaseID = 0;
	Plugin_Communication(CUSTOM_BASE_IS_DOCKED, &info);
	return info.iDockedBaseID;
}
uint GetCustomBaseForBase(unsigned int iBase)
{
	// Pass to plugins incase this ship is docked at a custom base.
	CUSTOM_GET_ME_POB_ID_STRUCT info;
	info.iBase = iBase;
	info.iPoBID = 0;
	Plugin_Communication(CUSTOM_GET_ME_POB_ID, &info);
	return info.iPoBID;
}
void __stdcall PlayerLaunch(unsigned int iShip, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;

	uint carrier_client = HkGetClientIdFromCharname(mobiledockClients[GChar(client)].wscDockedWithCharname);
	wstring carrier_filename = GCharByName(mobiledockClients[GChar(client)].wscDockedWithCharname);

	//snubs from offline carrier send to proxy
	if (carrier_client == -1 || mobiledockClients[GChar(client)].Disconnected && !mobiledockClients[GChar(client)].isCargoEmpty)
	{
		mobiledockClients[carrier_filename].mapDockedShips.erase(GCharNameByClient(client));
		mobiledockClients[carrier_filename].iDockingModulesAvailable++;
		mobiledockClients.erase(GChar(client));
		return;
	}

	wstring clientName = (const wchar_t*)Players.GetActiveCharacterName(client);
	if (mobiledockClients[GChar(client)].mobileDocked)
	{
		returncode = SKIPPLUGINS;
		// Set last base to last real base this ship was on. POB support will be added in 0.9.X version.
		// Actually POB langing works great without it. What is it for?!
		Players[client].iLastBaseID = mobiledockClients[GChar(client)].iLastBaseID;

		// Check if carrier died.
		if (mobiledockClients[GChar(client)].carrierDied)
		{
			// If died carrier and docked ship are in same system.
			if (Players[client].iSystemID == mobiledockClients[GChar(client)].carrierSystem)
			{
				// Now that the data is prepared, send the player to the carrier's location
				undockingShip = mobiledockClients[GChar(client)];

				// Clear the client out of the mobiledockClients now that it's no longer docked
				mobiledockClients.erase(GChar(client));
			}
			// If not - redirect to proxy base in carrier's system.
			else
			{
				wstring scProxyBase = HkGetSystemNickByID(mobiledockClients[GChar(client)].carrierSystem) + L"_proxy_base";
				if (pub::GetBaseID(mobiledockClients[GChar(client)].proxyBaseID, (wstos(scProxyBase)).c_str()) == -4)
				{
					PrintUserCmdText(client, L"No proxy base in system detected. Contact a developer about this please. Required base: %ws", scProxyBase);
					return;
				}

				mobiledockClients[GChar(client)].carrierSystem = -1;
			}
			return;
		}
		//Get the carrier ship information
		uint carrierShip;
		pub::Player::GetShip(carrier_client, carrierShip);
		uint clientShip;
		pub::Player::GetShip(client, clientShip);

		// Check to see if the carrier is currently in a base. If so, force the client to dock on that base.
		if (carrier_client == -1 || mobiledockClients[carrier_filename].IsCarrierInBase || mobiledockClients[GChar(client)].IsInsideOfflineCarrierInBase)
		{
			if (mobiledockClients[GChar(client)].DockedSomeBasesWhileInCarrier)
			{
				mobiledockClients[carrier_filename].mapDockedShips.erase(clientName);
				mobiledockClients[GChar(client)].baseUndock = true;
				return;
			}
		}
		// If carrier and docked ship are in same system. undock to space.
		if (Players[client].iSystemID == Players[carrier_client].iSystemID)
		{
			// Update the internal values of the docked ship pretaining to the carrier
			UpdateCarrierLocationInformation(client, carrierShip);

			// Now that the data is prepared, send the player to the carrier's location
			undockingShip = mobiledockClients[GChar(client)];

			// Clear the client out of the mobiledockClients now that it's no longer docked
			mobiledockClients.erase(GChar(client));

			// Remove charname from carrier's mapDockedShips.
			mobiledockClients[carrier_filename].mapDockedShips.erase(clientName);
			mobiledockClients[carrier_filename].iDockingModulesAvailable++;
		}
		// If not - redirect to proxy base in carrier's system.
		else
		{
			mobiledockClients[GChar(client)].carrierSystem = -1;
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
	//ConPrint(L"PlayerAfter0 = %u,%u\n", mobiledockClients[GChar(client)].undockNPCBase->iBaseID, mobiledockClients[GChar(client)].undockPoBID);
	if (mobiledockClients[GChar(client)].baseUndock)
	{
		uint systemID = Players[client].iSystemID;
		ImprovedForcedLand(client);
		if (mobiledockClients[GChar(client)].undockNPCBase)
			if (mobiledockClients[GChar(client)].undockNPCBase->iSystemID != systemID)
			{
				// Update current system stat in player list to be displayed relevantly.
				Server.BaseEnter(mobiledockClients[GChar(client)].undockNPCBase->iBaseID, client);
				Server.BaseExit(mobiledockClients[GChar(client)].undockNPCBase->iBaseID, client);
				wstring wscCharFileName;
				HkGetCharFileName((const wchar_t*)Players.GetActiveCharacterName(client), wscCharFileName);
				wscCharFileName += L".fl";
				CHARACTER_ID cID;
				strcpy(cID.szCharFilename, wstos(wscCharFileName.substr(0, 14)).c_str());
				Server.CharacterSelect(cID, client);
			}

		mobiledockClients.erase(GChar(client));
	}
	// Land ship to proxy base in carrier's system if they are in different systems.
	if (mobiledockClients[GChar(client)].carrierSystem == -1)
	{
		if (!mobiledockClients[GChar(client)].carrierDied)
		{
			string scProxyBase = HkGetPlayerSystemS(HkGetClientIdFromCharname(mobiledockClients[GChar(client)].wscDockedWithCharname)) + "_proxy_base";
			if (pub::GetBaseID(mobiledockClients[GChar(client)].proxyBaseID, scProxyBase.c_str()) == -4)
			{
				PrintUserCmdText(client, L"No proxy base in system detected. Contact a developer about this please. Required base: %s", scProxyBase);
				return;
			}
		}
		pub::Player::ForceLand(client, mobiledockClients[GChar(client)].proxyBaseID);

		// Send the message because if carrier goes to another system, docked ships remain in previous with outdated system navmap. We notify client about it is being updated.
		PrintUserCmdText(client, L"Navmap updated successfully.");

		// Update current system stat in player list to be displayed relevantly.
		Server.BaseEnter(mobiledockClients[GChar(client)].proxyBaseID, client);
		Server.BaseExit(mobiledockClients[GChar(client)].proxyBaseID, client);
		wstring wscCharFileName;
		HkGetCharFileName((const wchar_t*)Players.GetActiveCharacterName(client), wscCharFileName);
		wscCharFileName += L".fl";
		CHARACTER_ID cID;
		strcpy(cID.szCharFilename, wstos(wscCharFileName.substr(0, 14)).c_str());
		Server.CharacterSelect(cID, client);

		// Update current system stat in plugin data.
		Universe::IBase* base = Universe::get_base(mobiledockClients[GChar(client)].proxyBaseID);
		mobiledockClients[GChar(client)].carrierSystem = base->iSystemID;
	}
}
void UpdateModuleInfo(uint iTargetClientID)
{
	// Check that the target ship has an empty docking module.
	mobiledockClients[GChar(iTargetClientID)].iDockingModulesInstalled = GetInstalledModules(iTargetClientID);

	// Normalize the docking modules available, with the number of people currently docked
	mobiledockClients[GChar(iTargetClientID)].iDockingModulesAvailable = (mobiledockClients[GChar(iTargetClientID)].iDockingModulesInstalled - mobiledockClients[GChar(iTargetClientID)].mapDockedShips.size());
}
float RealDistance(uint iTargetShip, uint iShip)
{
	Vector radiusVector{};
	float carrierRadius;
	pub::SpaceObj::GetRadius(iTargetShip, carrierRadius, radiusVector);
	float HKDistance = HkDistance3DByShip(iShip, iTargetShip);
	return (carrierRadius < HKDistance) ? HKDistance - carrierRadius : HKDistance;
}
int RequestForDock(uint client, uint iShip)
{
	if (client)
	{
		// If no target then ignore the request.
		uint iTargetShip;
		pub::SpaceObj::GetTarget(iShip, iTargetShip);
		if (!iTargetShip)
		{
			return 0;
		}

		uint iType;
		pub::SpaceObj::GetType(iTargetShip, iType);
		if (iType != OBJ_FREIGHTER)
		{
			return 0;
		}

		// If target is not player ship or ship is too far away then ignore the request.
		const uint iTargetClientID = HkGetClientIDByShip(iTargetShip);
		float distance = RealDistance(iTargetShip, iShip);
		if (!iTargetClientID || distance > 1050.0f)
		{
			PrintUserCmdText(client, L"Ship distance %f is out of range", distance);
			return 0;
		}

		UpdateModuleInfo(iTargetClientID);
		if (mobiledockClients[GChar(iTargetClientID)].iDockingModulesAvailable == 0)
		{
			PrintUserCmdText(client, L"Target ship has no free docking capacity");
			return 0;
		}

		// Check that the requesting ship is of the appropriate size to dock.
		CShip* cship = dynamic_cast<CShip*>(HkGetEqObjFromObjRW(reinterpret_cast<IObjRW*>(HkGetInspect(client))));
		if (cship->shiparch()->fHoldSize > cargoCapacityLimit)
		{
			PrintUserCmdText(client, L"Target ship cannot dock a ship of your size. Maximum limit %d cargo sized ships", cargoCapacityLimit);
			return 0;
		}

		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		// Create a docking request and send a notification to the target ship.
		mapPendingDockingRequests[client] = iTargetClientID;
		PrintUserCmdText(iTargetClientID, L"%s is requesting to dock, authorise with /dock allow", Players.GetActiveCharacterName(client));
		PrintUserCmdText(client, L"Docking request sent to %s", Players.GetActiveCharacterName(iTargetClientID));

		wstring wscMsg = L"%time Traffic control alert: %player has requested to dock";
		wscMsg = ReplaceStr(wscMsg, L"%time", GetTimeString(set_bLocalTime));
		wscMsg = ReplaceStr(wscMsg, L"%player", (const wchar_t*)Players.GetActiveCharacterName(client));
		PrintLocalUserCmdText(client, wscMsg, set_iLocalChatRange);
		return -1;
	}
	return 0;
}

// If this is a docking request at a player ship then process it.
int __cdecl Dock_Call(unsigned int const &iShip, unsigned int const &iBaseID, int iCancel, enum DOCK_HOST_RESPONSE response)
{
	returncode = DEFAULT_RETURNCODE;

	UINT client = HkGetClientIDByShip(iShip);
	return RequestForDock(client, iShip);
}
bool ValidateCargo(unsigned int client)
{
	std::wstring playerName = (const wchar_t*)Players.GetActiveCharacterName(client);
	std::list<CARGO_INFO> cargo;
	int holdSize = 0;

	HkEnumCargo(playerName, cargo, holdSize);

	for (std::list<CARGO_INFO>::const_iterator it = cargo.begin(); it != cargo.end(); ++it)
	{
		const CARGO_INFO & item = *it;

		bool flag = false;
		pub::IsCommodity(item.iArchID, flag);

		// Some commodity present.
		if (flag)
			return false;
	}

	return true;
}
void __stdcall BaseEnter(uint iBaseID, uint client)
{
	returncode = DEFAULT_RETURNCODE;
	wstring FileCharName = GChar(client);
	mobiledockClients[FileCharName].IsCarrierInBase = true;
	if (mobiledockClients[FileCharName].iDockingModulesInstalled != 0)
	{
		mobiledockClients[FileCharName].undockPoBID = GetCustomBaseForClient(client);
		mobiledockClients[FileCharName].undockNPCBase = Universe::get_base(iBaseID);
	}

	if (mobiledockClients.find(FileCharName) != mobiledockClients.end())
	{
		if (!mobiledockClients[FileCharName].mapDockedShips.empty())
		{
			// Send a system switch to force each ship to launch
			for (map<wstring, wstring>::iterator i = mobiledockClients[FileCharName].mapDockedShips.begin();
				i != mobiledockClients[FileCharName].mapDockedShips.end(); ++i)
			{
				wstring DockedCharfile = GCharByName(i->second);

				mobiledockClients[DockedCharfile].undockPoBID = GetCustomBaseForClient(client);
				mobiledockClients[DockedCharfile].undockNPCBase = Universe::get_base(iBaseID);


				//Remember that carrier was docking some bases with those snubs
				mobiledockClients[DockedCharfile].DockedSomeBasesWhileInCarrier = true;
			}
		}
	}

	if (mobiledockClients[FileCharName].mobileDocked)
	{
		if (!mobiledockClients[GChar(client)].isCargoEmpty)
			PrintUserCmdText(client, L"Alert! You have cargo inside your ship. Docking module is turned into unstable state. Exit by offline way carried snub is possible only to proxy base.", mobiledockClients[GChar(client)].wscDockedWithCharname);

		// Clear the market. We don't support transfers in this version.
		SendResetMarketOverride(client);

		//Copy item list from some base to have them inside docking module
		list<uint> Items;
		uint iBaseID = Universe::get_base_id(shop_inside.c_str()); //Copying commodities from Green Station
		HkGetItemsForSale(iBaseID, Items);

		uint iBaseID2;
		string scProxyBase = HkGetPlayerSystemS(client) + "_proxy_base";
		if (pub::GetBaseID(iBaseID2, scProxyBase.c_str()) == -4)
		{
			PrintUserCmdText(client, L"No proxy base in system detected. Contact a developer about this please. Required base: %s", scProxyBase);
		}
		
		uint BadItem = 0;
		//Deleting cloak, jump and anomaly batteries.
		pub::GetGoodID(BadItem, "missile01_mark04_ammo"); Items.remove(BadItem);
		pub::GetGoodID(BadItem, "missile01_mark01_ammo"); Items.remove(BadItem);
		pub::GetGoodID(BadItem, "missile01_mark02_ammo"); Items.remove(BadItem);
		pub::GetGoodID(BadItem, "mine01_mark01_ammo"); Items.remove(BadItem);
		pub::GetGoodID(BadItem, "dsy_anomalyscanner01_ammo"); Items.remove(BadItem);

		//Override market with items/prices from chosen station
		std::list<uint>::iterator it;
		for (it = Items.begin(); it != Items.end(); ++it) {
			wchar_t buf[200];
			float fItemValue;
			uint iGoodID = (*it);
			pub::Market::GetPrice(iBaseID, Arch2Good(iGoodID), fItemValue);
			_snwprintf(buf, sizeof(buf), L" SetMarketOverride %u %u %f %u %u",
				iBaseID2, (*it), fItemValue, 0, 5000);

			SendCommand(client, buf);
		}


		// Set the base name in infocard
		wstring status = L"<RDL><PUSH/>";
		status += L"<TEXT>" + XMLText(mobiledockClients[GChar(client)].wscDockedWithCharname) + L"</TEXT><PARA/><PARA/>";
		status += L"<POP/></RDL>";
		SendSetBaseInfoText2(client, status);
		PrintUserCmdText(client, L"You are in %s's docking bay!", mobiledockClients[GChar(client)].wscDockedWithCharname.c_str());
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
			if (!mobiledockClients[GChar(client)].mapDockedShips.empty())
			{
				// Send a system switch to force each ship to launch
				for (map<wstring, wstring>::iterator i = mobiledockClients[GChar(client)].mapDockedShips.begin();
					i != mobiledockClients[GChar(client)].mapDockedShips.end(); ++i)
				{
					uint iDockedClientID = HkGetClientIdFromCharname(i->second);

					if (iDockedClientID != -1)
					{
						// Update the coordinates the given ship should launch to.
						UpdateCarrierLocationInformation(iDockedClientID, cship->get_position(), cship->get_orientation());

						// Carrier is no more. Set the flag.
						mobiledockClients[GChar(iDockedClientID)].carrierDied = true;

						// Due to the carrier not existing anymore, we have to pull the system information from the carriers historical location.
						mobiledockClients[GChar(iDockedClientID)].carrierSystem = cship->iSystem;

						JettisonShip(client, iDockedClientID);
					}
					else //those are offline ships
					{
						wstring wscCharFileName = GCharByName(i->second);

						//if docked something with carrier, send to base, otherwise to proxy.
						if (mobiledockClients[wscCharFileName].DockedSomeBasesWhileInCarrier)
							//undock at base last visited by carrier
							mobiledockClients[wscCharFileName].IsInsideOfflineCarrierInBase = true;
						else //erasy their access and send to proxy otherwise
							mobiledockClients.erase(wscCharFileName);
					}
				}

				// Clear the carrier from the list
				mobiledockClients[GChar(client)].mapDockedShips.clear();
				mobiledockClients[GChar(client)].iDockingModulesAvailable = mobiledockClients[GChar(client)].iDockingModulesInstalled;
			}
		}
	}
}


bool UserCmd_Process(uint client, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;
	
	if (wscCmd.find(L"/conn") == 0 || wscCmd.find(L"/return") == 0)
	{
	// This plugin always runs before the Conn Plugin runs it's /conn function. Verify that there are no docked ships.
	//Btw it can be not true.
	if (!mobiledockClients[GChar(client)].mapDockedShips.empty())
	{
		PrintUserCmdText(client, L"You cannot use this command if you have vessels docked with you!");
		returncode = SKIPPLUGINS;
		return true;
	}
	}
	else if (wscCmd.find(L"/dock") == 0)
	{
		const wstring &cmd = GetParam(wscCmd, ' ', 1);

		if (cmd == L"list")
		{
			PrintUserCmdText(client, L"%d out of %u dockings bays available.", mobiledockClients[GChar(client)].iDockingModulesAvailable, mobiledockClients[GChar(client)].iDockingModulesInstalled);
			if (mobiledockClients[GChar(client)].mapDockedShips.empty())
			{
				PrintUserCmdText(client, L"No ships currently docked");
			}
			else
			{
				PrintUserCmdText(client, L"Docked ships (%d):", mobiledockClients[GChar(client)].mapDockedShips.size());
				int number = 1;
				for (map<wstring, wstring>::iterator i = mobiledockClients[GChar(client)].mapDockedShips.begin();
					i != mobiledockClients[GChar(client)].mapDockedShips.end(); ++i)
				{
					PrintUserCmdText(client, L"#%d, %s", number, i->first.c_str()); number++;
				}
			}
			return true;
		}
		else if (cmd == L"invite")
		{
			uint index = ToInt(GetParam(wscCmd, ' ', 2));
			if (index <= 0 || index > 100000)
			{
				PrintUserCmdText(client, L"Usage: /dock invite <#number 0 - 100000>");
				return true;
			}

			uint iShip;
			pub::Player::GetShip(client, iShip);
			UpdateModuleInfo(client);
			if (mobiledockClients[GChar(client)].iDockingModulesAvailable == 0)
			{
				PrintUserCmdText(client, L"Target ship has no free docking capacity");
				return 0;
			}

			uint iBaseID;
			pub::Player::GetBase(client, iBaseID);
			if (iBaseID == 0)
			{
				PrintUserCmdText(client, L"You must be docked to base to do it.");
				return 0;
			}

			string scProxyBase = HkGetPlayerSystemS(client) + "_proxy_base";
			uint iBaseID2;
			if (pub::GetBaseID(iBaseID2, scProxyBase.c_str()) == -4)
			{
				PrintUserCmdText(client, L"No proxy base in system detected. Contact a developer about this please. Required base: %s", scProxyBase);
				return true;
			}
			//Player in proxy, means in POB
			if (iBaseID == iBaseID2)
			{
				iBaseID = GetCustomBaseForClient(client);
			}

			mobiledockClients[GChar(client)].IsCarrierInBase = true;

			wstring carrierfilename = GChar(client);
			InviteLink tempo;
			tempo.Carrier = carrierfilename;
			tempo.carrierwchar = (const wchar_t*)Players.GetActiveCharacterName(client);
			tempo.CarrierWCharName = (const wchar_t*)Players.GetActiveCharacterName(client);
			tempo.Password = index;
			tempo.iBase = iBaseID;
			tempo.Time = (uint)time(0);

			//Clean from expired links.
			list<InviteLink> RefreshedList;
			std::list<InviteLink>::iterator it;
			for (it = mapInviteLinks[iBaseID].begin(); it != mapInviteLinks[iBaseID].end(); ++it) {
				if (((uint)time(0) - it->Time) < ExpirationOfflineInvitation)
				{
					RefreshedList.push_back(*it);
					break;
				}
			}
			mapInviteLinks.erase(iBaseID);				
			mapInviteLinks[iBaseID]	= RefreshedList;
			mapInviteLinks[iBaseID].push_back(tempo);
			
			PrintUserCmdText(client, L"OK invite link %u has been created to be active for %d seconds.", index, ExpirationOfflineInvitation);
			return true;
		}
		else if (cmd == L"accept")
		{
			uint index = ToInt(GetParam(wscCmd, ' ', 2));
			if (index <= 0 || index > 100000)
			{
				PrintUserCmdText(client, L"Usage: /dock accept <#number 0 - 100000>");
				return true;
			}
			uint iShip;
			pub::Player::GetShip(client, iShip);
			uint iBase;
			pub::Player::GetBase(client, iBase);
			if (iBase != 0)
			{
				PrintUserCmdText(client, L"ERR You must be in space selecting base with docked carrier.");
				return 0;
			}
			uint iTargetID;
			pub::SpaceObj::GetTarget(iShip, iTargetID);
			float distance = RealDistance(iTargetID, iShip);
			if (!iTargetID || distance > 1050.0f)
			{
				PrintUserCmdText(client, L"Base distance %f is out of range", distance);
				return true;
			}
			uint iDockingTargetID;
			pub::SpaceObj::GetDockingTarget(iTargetID, iDockingTargetID);
			
			uint iBaseID;
			bool ItisPoB = false;
			if (iDockingTargetID==0)
			{
				//It is a pob
				ItisPoB = true;
				iBaseID = iTargetID;
			}
			else iBaseID = iDockingTargetID;
			
			if (mapInviteLinks.find(iBaseID) == mapInviteLinks.end())
			{
				PrintUserCmdText(client, L"ERR No invitation has been found.");
				return 0;
			}
			if (mapInviteLinks[iBaseID].empty())
			{
				PrintUserCmdText(client, L"ERR No invite links.");
				return 0;
			}
			bool found = false;
			std::list<InviteLink>::iterator it;
			for (it = mapInviteLinks[iBaseID].begin(); it != mapInviteLinks[iBaseID].end(); ++it) {
				if (it->Password == index && ((uint)time(0) - it->Time) < ExpirationOfflineInvitation)
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				PrintUserCmdText(client, L"ERR Incorrect password, base or expired.");
				return 0;
			}
			string scProxyBase = HkGetPlayerSystemS(client) + "_proxy_base";
			uint iBaseID2;
			if (pub::GetBaseID(iBaseID2, scProxyBase.c_str()) == -4)
			{
				PrintUserCmdText(client, L"No proxy base in system detected. Contact a developer about this please. Required base: %s", scProxyBase);
				return true;
			}

			// Check that there is an empty docking module
			if (mobiledockClients[it->Carrier].iDockingModulesAvailable <= 0)
			{
				PrintUserCmdText(client, L"No free docking modules available.");
				return true;
			}
			if (ItisPoB)
			{
				if (mobiledockClients[it->Carrier].undockPoBID != iTargetID)
				{
					PrintUserCmdText(client, L"ERR Code 1.1 The Carrier traveled elsewhere.");
					return true;
				}
			}
			else
			{
				if (mobiledockClients[it->Carrier].undockNPCBase != 0 && mobiledockClients[it->Carrier].undockNPCBase != NULL)
					if (mobiledockClients[it->Carrier].undockNPCBase->iBaseID != iDockingTargetID)
					{
						PrintUserCmdText(client, L"ERR Code 1.2 The Carrier traveled elsewhere.");
						return true;
					}
			}
			if (it->carrierwchar != NULL)
			{
				uint checkCarrierClientID = HkGetClientIdFromCharname(it->carrierwchar);
				if (checkCarrierClientID != -1 && !mobiledockClients[it->Carrier].IsCarrierInBase)
				{
					wstring charname = (const wchar_t*)Players.GetActiveCharacterName(checkCarrierClientID);
					if (!(charname.compare(it->CarrierWCharName)))
					{
						PrintUserCmdText(client, L"ERR Code 1.3 The Carrier is not docked to base.");
							return true;
					}
				}
			}
			// Check that the requesting ship is of the appropriate size to dock.
			CShip* cship = dynamic_cast<CShip*>(HkGetEqObjFromObjRW(reinterpret_cast<IObjRW*>(HkGetInspect(client))));
			if (cship->shiparch()->fHoldSize > cargoCapacityLimit)
			{
				PrintUserCmdText(client, L"Target ship cannot dock a ship of your size. Maximum limit %d cargo sized ships", cargoCapacityLimit);
				return 0;
			}

			// Save the carrier info
			wstring charname = (const wchar_t*)Players.GetActiveCharacterName(client);
			mobiledockClients[it->Carrier].mapDockedShips[charname] = charname;
			mobiledockClients[it->Carrier].clientID = client;
			pub::SpaceObj::GetSystem(iShip, mobiledockClients[it->Carrier].carrierSystem);

			// Save the docking ship info
			mobiledockClients[GChar(client)].mobileDocked = true;
			mobiledockClients[GChar(client)].clientID = client;
			mobiledockClients[GChar(client)].wscDockedWithCharname = it->CarrierWCharName;
			mobiledockClients[GChar(client)].iLastBaseID = Players[client].iLastBaseID;
			mobiledockClients[GChar(client)].proxyBaseID = iBaseID2;
			mobiledockClients[GChar(client)].isCargoEmpty = ValidateCargo(client);
			mobiledockClients[GChar(client)].Disconnected = false;
			pub::SpaceObj::GetSystem(iShip, mobiledockClients[GChar(client)].carrierSystem);

			// Substract available info
			mobiledockClients[it->Carrier].iDockingModulesAvailable--;

			//Traffic control
			wstring wscMsg = L"%time Traffic control alert: %player has requested to dock";
			wscMsg = ReplaceStr(wscMsg, L"%time", GetTimeString(set_bLocalTime));
			wscMsg = ReplaceStr(wscMsg, L"%player", (const wchar_t*)Players.GetActiveCharacterName(client));
			PrintLocalUserCmdText(client, wscMsg, set_iLocalChatRange);

			//Land the ship.
			pub::Player::ForceLand(client, iBaseID2);
			PrintUserCmdText(client, L"Ship docked");
			
			//Record the base where carrier is, for fighter undocking purposes
			mobiledockClients[GChar(client)].undockPoBID = iBaseID;
			mobiledockClients[GChar(client)].undockNPCBase = Universe::get_base(iBaseID);
			mobiledockClients[GChar(client)].DockedSomeBasesWhileInCarrier = true;
		}
		else if (cmd == L"send")
		{
			//Alternative to F3 docking. Works better due to being not restricted by F3 mechanics
			uint iShip;
			pub::Player::GetShip(client, iShip);
			if (RequestForDock(client, iShip) == 0)
				return true;
		}
		else if (cmd == L"allow")
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
			float distance = RealDistance(iShip, iTargetShip);
			if (!iTargetClientID || distance > 1050.0f)
			{
				PrintUserCmdText(client, L"Ship distance %f is out of range", distance);
				return true;
			}

			// Find the docking request. If none, ignore.
			if (mapPendingDockingRequests.find(iTargetClientID) == mapPendingDockingRequests.end())
			{
				PrintUserCmdText(client, L"No pending docking requests for this ship");
				return true;
			}

			// Check that there is an empty docking module
			if (mobiledockClients[GChar(client)].iDockingModulesAvailable <= 0)
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
			mobiledockClients[GChar(client)].mapDockedShips[charname] = charname;
			mobiledockClients[GChar(client)].clientID = client;
			pub::SpaceObj::GetSystem(iShip, mobiledockClients[GChar(client)].carrierSystem);
			if (mobiledockClients[GChar(client)].iLastBaseID != 0)
				mobiledockClients[GChar(client)].iLastBaseID = Players[client].iLastBaseID;

			// Save the docking ship info
			mobiledockClients[GChar(iTargetClientID)].mobileDocked = true;
			mobiledockClients[GChar(iTargetClientID)].clientID = iTargetClientID;
			mobiledockClients[GChar(iTargetClientID)].wscDockedWithCharname = (const wchar_t*)Players.GetActiveCharacterName(client);
			mobiledockClients[GChar(iTargetClientID)].iLastBaseID = Players[iTargetClientID].iLastBaseID;
			mobiledockClients[GChar(iTargetClientID)].proxyBaseID = iBaseID;
			mobiledockClients[GChar(iTargetClientID)].isCargoEmpty = ValidateCargo(iTargetClientID);
			mobiledockClients[GChar(iTargetClientID)].Disconnected = false;
			
			pub::SpaceObj::GetSystem(iShip, mobiledockClients[GChar(iTargetClientID)].carrierSystem);

			mobiledockClients[GChar(client)].iDockingModulesAvailable--;

			// Land the ship on the proxy base
			pub::Player::ForceLand(iTargetClientID, iBaseID);
			PrintUserCmdText(client, L"Ship docked");

			return true;
		}
		else if (cmd == L"kick")
		{
			// Get the supposed ship we should be ejecting from the command parameters
			uint index = ToInt(GetParam(wscCmd, ' ', 2));
			if (index <= 0 || index > mobiledockClients[GChar(client)].mapDockedShips.size())
			{
				PrintUserCmdText(client, L"Usage: /dock kick <#number>");
				return true;
			}

			// Only allow jettisonning a ship if the carrier is undocked
			uint carrierShip;
			pub::Player::GetShip(client, carrierShip);

			//Find ship to kick
			uint number = 1; 
			wstring charname = L"";
			for (map<wstring, wstring>::iterator i = mobiledockClients[GChar(client)].mapDockedShips.begin();
				i != mobiledockClients[GChar(client)].mapDockedShips.end(); ++i)
			{
				charname = i->first; number++;

				if (number == index) break;
			}
			PrintUserCmdText(client, L"%s is going to be kicked!", charname.c_str());

			// Check to see if the user listed is actually docked with the carrier at the moment
			if (mobiledockClients[GChar(client)].mapDockedShips.find(charname) == mobiledockClients[GChar(client)].mapDockedShips.end())
			{
				PrintUserCmdText(client, L"%s is not docked with you!", charname.c_str());
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
			else
			{
				//Otherwise do something with him like he is offline player.
				wstring wscCarrierCharFileName = GChar(client);
				wstring wscCharFileName = GCharByName(charname);

				//restore amount of free modules to carrier
				if (mobiledockClients[wscCarrierCharFileName].mapDockedShips.find(charname) != mobiledockClients[wscCarrierCharFileName].mapDockedShips.end())
				{
					//if docked something with carrier, send to base, otherwise to proxy.
					if (mobiledockClients[wscCharFileName].DockedSomeBasesWhileInCarrier && mobiledockClients[wscCharFileName].isCargoEmpty)
					{
						mobiledockClients[wscCharFileName].IsInsideOfflineCarrierInBase = true;
						mobiledockClients[wscCarrierCharFileName].mapDockedShips.erase(charname);
						mobiledockClients[wscCarrierCharFileName].iDockingModulesAvailable++;
						PrintUserCmdText(client, L"OK Ship is jettisoned to nearby planet.");
					}
					else
					{
						mobiledockClients.erase(wscCharFileName);
						PrintUserCmdText(client, L"OK Ship is jettisoned to somewhere.");
					}

					
				}
				else
					PrintUserCmdText(client, L"ERR Ship is not found.");
			}

			return true;
		}
		else
		{
			PrintUserCmdText(client, L"ERR Invalid parameters");
			PrintUserCmdText(client, L"/dock [send|allow|list|kick|invite|accept]");
			PrintUserCmdText(client, L"|  send - initiate attempt to dock selected carrier (Alt. to F3)");
			PrintUserCmdText(client, L"|  allow - select snub attempting to dock and accept");
			PrintUserCmdText(client, L"|  list - show list of docked snubs");
			PrintUserCmdText(client, L"|  kick <Number> - kick chosen snub");
			PrintUserCmdText(client, L"|  invite <Any Number from 0 to 100000> - create invitation for offline docking");
			PrintUserCmdText(client, L"|  accept <Matching Number> - select base where offline carrier is and accept invitation for offline docking");
		}
		
	}
	
	return false;
}

void __stdcall DisConnect(uint iClientID, enum EFLConnection p2)
{
	returncode = DEFAULT_RETURNCODE;
	mobiledockClients[GChar(iClientID)].Disconnected = true;
}

void __stdcall CharacterSelect_AFTER(struct CHARACTER_ID const & cId, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	
	wstring CharFile = GChar(iClientID);

	mobiledockClients[CharFile].clientID = iClientID;

	// Update count of installed modules in case if client left his ship in open space before.
	mobiledockClients[CharFile].iDockingModulesAvailable = mobiledockClients[GChar(iClientID)].iDockingModulesInstalled = GetInstalledModules(iClientID);
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
		for (map<wstring, CLIENT_DATA>::iterator it = mobiledockClients.begin(); it != mobiledockClients.end(); ++it)
		{
			wstring charname;
			
			
			try
			{
				charname = (const wchar_t*)Players.GetActiveCharacterName(it->second.clientID);
			}
			catch (...)
			{
				charname = L"<Error>";
			}

			string ID = to_string(it->second.clientID);
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

			if (it->second.clientID == 0 || it->second.clientID > MAX_CLIENT_ID)
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
