/**
 * Mobile Docking Plugin for FLHook
 * Initial Version by Cannon
 * Using some work written by Alley
 * Rework by Remnant
 * Resurrected by Invoker
 */

#include "Main.h"

PLUGIN_RETURNCODE returncode;

// Lists of clients, which are part of dock plugin now.
map<uint, CLIENT_DATA> mobiledockClients;
map<uint, uint> mapPendingDockingRequests;
vector<CLIENT_DATA> undockingQueue;

// Lists of items, used in dock plugin.
uint dockingModuleEquipmentID;
uint crewGoodID;
map<uint, AMMO> mapAmmo;
map<uint, uint> mapBatteries;
uint nanobotsID = CreateID("ge_s_repair_01");
uint batteriesID = CreateID("ge_s_battery_01");
vector<uint> boostedAmmo;
map<uint, SUPPLY> mapSupplies;

// Main constants defining behavior of this plugin.
int cargoCapacityLimit = 180;
int jettisonKickTime = 15;
int groupDockDelay = 2;
float dockDistance = 300.0f;
int undockDistance = 100;
int crewMinimum = 100;
int crewEfficienyConst = 2;

// Delayed actions, which need to be done.
vector<Task> TaskScheduler;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Updates carrier position for client, if both are online.
void UpdateCarrierLocationInformation(uint dockedClientId, uint carrierShip) 
{
	// Prepare references to the docked ship's copy of the carrierShip's position and rotation for manipulation.
	Vector& carrierPos = mobiledockClients[dockedClientId].carrierPos;
	Matrix& carrierRot = mobiledockClients[dockedClientId].carrierRot;
		
	// If the carrier is out in space, simply set the undock location to where the carrier is currently.
	pub::SpaceObj::GetSystem(carrierShip, mobiledockClients[dockedClientId].carrierSystem);
	pub::SpaceObj::GetLocation(carrierShip, carrierPos, carrierRot);
}

// Overloaded function used with a specific carrier location, instead of extracting it from the ship itself.
void UpdateCarrierLocationInformation(uint dockedClientId, Vector pos, Matrix rot)
{
	mobiledockClients[dockedClientId].carrierPos = pos;
	mobiledockClients[dockedClientId].carrierRot = rot;
}

// Returns correct undocking position above carrier ship.
Vector GetUndockingPosition(Vector carrierPos, Matrix carrierRot)
{
	Vector undockPos;

	undockPos.x = carrierPos.x + carrierRot.data[0][1] * undockDistance;
	undockPos.y = carrierPos.y + carrierRot.data[1][1] * undockDistance;
	undockPos.z = carrierPos.z + carrierRot.data[2][1] * undockDistance;

	return undockPos;
}

// Get list of docked ships in string format.
wstring EnumerateDockedShips(uint carrierClientID)
{
	if (!mobiledockClients[carrierClientID].dockedShips.empty())
	{
		wstring shipNames;

		for (int i = 0; i != mobiledockClients[carrierClientID].dockedShips.size(); i++)
		{
			if (i != 0)
				shipNames += L", ";

			shipNames += mobiledockClients[carrierClientID].dockedShips[i];
		}

		return L"Detected ships on board: " + shipNames + L".";
	}
	else if (mobiledockClients[carrierClientID].iDockingModulesInstalled == 0)
	{
		return L"No docking modules detected.";
	}

	return L"No docked ships detected on board.";
}

// Get proxy base in system where the client currently located.
uint GetProxyBaseForClient(uint client)
{
	string scProxyBase = HkGetPlayerSystemS(client) + "_proxy_base";
	uint iBaseID;
	if (pub::GetBaseID(iBaseID, scProxyBase.c_str()) == -4)
	{
		PrintUserCmdText(client, L"No proxy base in system detected. Contact a developer about this please.");
		return -1;
	}
	return iBaseID;
}

// Get proxy base ID in specific system
uint GetProxyBaseForSystem(uint systemID)
{
	uint iBaseID;
	wstring proxyBase = HkGetSystemNickByID(systemID) + L"_proxy_base";
	pub::GetBaseID(iBaseID, (wstos(proxyBase)).c_str());

	return iBaseID;
}

// Jettisons ship from carrier.
void JettisonShip(uint carrierClientID, wstring dockedCharname, bool eraseFromList)
{
	const wchar_t* carrierCharname = (const wchar_t*)Players.GetActiveCharacterName(carrierClientID);
	const uint iDockedClientID = HkGetClientIdFromCharname(dockedCharname);
	if (iDockedClientID != -1)
	{
		PrintUserCmdText(carrierClientID, L"Ship warned. If it won't undock in %i seconds, it will be kicked by force.", jettisonKickTime);
		PrintUserCmdText(iDockedClientID, L"Carrier wants to jettison your ship. Undock willingly or you will be kicked after %i seconds.", jettisonKickTime);
		pub::Audio::PlaySoundEffect(iDockedClientID, CreateID("rtc_klaxon_loop"));

		TaskScheduler.push_back(Task(jettisonKickTime, [dockedCharname, carrierCharname, eraseFromList]()
		{
			uint checkCarrierClientID = HkGetClientIdFromCharname(carrierCharname);
			uint checkDockedClientID = HkGetClientIdFromCharname(dockedCharname);

			// If both carrier and docked ship are still at server.
			if (checkDockedClientID != -1 && checkCarrierClientID != -1)
			{
				if (mobiledockClients[checkDockedClientID].wscDockedWithCharname == carrierCharname)
				{
					if (eraseFromList)
					{
						vector<wstring> &dockedShips = mobiledockClients[checkCarrierClientID].dockedShips;
						dockedShips.erase(std::remove(dockedShips.begin(), dockedShips.end(), dockedCharname), dockedShips.end());
					}
					mobiledockClients.erase(checkDockedClientID);
					HkKick(HkGetAccountByClientID(checkDockedClientID));
					JettisonShipOffline(checkCarrierClientID, dockedCharname);
				}
			}
			// If only carrier ship at server.
			else if (checkCarrierClientID != -1)
			{
				if (eraseFromList)
				{
					vector<wstring> &dockedShips = mobiledockClients[checkCarrierClientID].dockedShips;
					dockedShips.erase(std::remove(dockedShips.begin(), dockedShips.end(), dockedCharname), dockedShips.end());
				}
				JettisonShipOffline(checkCarrierClientID, dockedCharname);
			}
			// If only docked ship at server.
			else if (checkDockedClientID != -1)
			{
				mobiledockClients.erase(checkDockedClientID);
				HkKick(HkGetAccountByClientID(checkDockedClientID));
				JettisonShipOffline(dockedCharname, carrierCharname);
			}
			// If things go insane and both are gone from server.		
			else
			{
				// Use overloaded function to do jettisoning 100% offline
				JettisonShipOffline(dockedCharname, carrierCharname);
			}
		}));
	}
	else
	// If only carrier ship at server.
	{
		vector<wstring> &dockedShips = mobiledockClients[carrierClientID].dockedShips;
		dockedShips.erase(std::remove(dockedShips.begin(), dockedShips.end(), dockedCharname), dockedShips.end());
		JettisonShipOffline(carrierClientID, dockedCharname);
	}
}

// Docks ship to carrier in space.
void DockShip(uint carrierShip, uint carrierClientID, uint dockingClientID)
{
	// If ship already docked - return.
	mapPendingDockingRequests.erase(dockingClientID);

	uint proxyBaseToDock = GetProxyBaseForClient(carrierClientID);

	// Save the carrier info.
	wstring dockingCharname = (const wchar_t*)Players.GetActiveCharacterName(dockingClientID);
	mobiledockClients[carrierClientID].dockedShips.push_back(dockingCharname);
	pub::SpaceObj::GetSystem(carrierShip, mobiledockClients[carrierClientID].carrierSystem);
	mobiledockClients[carrierClientID].iLastBaseID = Players[carrierClientID].iLastBaseID;

	// Save the docking ship info.
	mobiledockClients[dockingClientID].iLastBaseID = Players[dockingClientID].iLastBaseID;
	mobiledockClients[dockingClientID].proxyBaseID = proxyBaseToDock;
	pub::SpaceObj::GetSystem(carrierShip, mobiledockClients[dockingClientID].carrierSystem);
	mobiledockClients[dockingClientID].wscDockedWithCharname = (const wchar_t*)Players.GetActiveCharacterName(carrierClientID);

	// Land docking ship on proxy base.
	pub::Player::ForceLand(dockingClientID, proxyBaseToDock);
	pub::Audio::PlaySoundEffect(carrierClientID, CreateID("tractor_loot_grabbed"));

	// Notify ship if carrier has supplies.
	SUPPLY_INFO info = SUPPLY_INFO();
	CheckIfResupplyingAvailable(carrierClientID, dockingClientID, info, true);
}

// Checks if carrier ship has all to resupply docked ship and returns time required for it. Notifies only about possibility if notify = true, or notifies only about impossibility if notify = false.
int CheckIfResupplyingAvailable(uint carrierClientID, uint dockedClientID, SUPPLY_INFO& info, bool notify)
{
	wstring carrierCharname = mobiledockClients[dockedClientID].wscDockedWithCharname;
	int crew = 0;

	list<CARGO_INFO> lstCargo;
	int iRemainingHoldSize = 0;
	HkEnumCargo(carrierCharname, lstCargo, iRemainingHoldSize);

	foreach(lstCargo, CARGO_INFO, item)
	{
		if (item->iArchID == crewGoodID)
			crew = item->iCount;

		else if (mapSupplies.find(item->iArchID) != mapSupplies.end())
		{
			if (mapSupplies[item->iArchID].type == 0)
			{
				info.hasAmmoSup = true; info.hasHullSup = true; info.hasShieldSup = true;
			}
			else if (mapSupplies[item->iArchID].type == 1)
			{
				info.hasAmmoSup = true;
			}
			else if (mapSupplies[item->iArchID].type == 2)
			{
				info.hasHullSup = true; info.hasShieldSup = true;
			}
			else if (mapSupplies[item->iArchID].type == 3)
			{
				info.hasShieldSup = true;
			}
			else if (mapSupplies[item->iArchID].type == 4)
			{
				info.hasHullSup = true;
			}
			else if (mapSupplies[item->iArchID].type == 5)
			{
				info.hasCloakSup = true;
			}
		}
	}

	if (crew < crewMinimum)
	{
		if(!notify)
			PrintUserCmdText(dockedClientID, L"Carrier ship has not enough crew.");
		return -1;
	}
	else if (!info.hasAmmoSup && !info.hasCloakSup && !info.hasHullSup && !info.hasShieldSup)
	{
		if (!notify)
			PrintUserCmdText(dockedClientID, L"Carrier has no supplies.");
		return -1;
	}
	else if (notify)
		PrintUserCmdText(dockedClientID, L"Resupplying system available. Insert '/loadsupplies' to use it.");

	return (int) (60.0f / pow(2, ((crew - 100) / 200.0f)));
}

// Updates count of installed modules for specific ship. Updates the count and returns it if need to compare.
uint UpdateAvailableModules(uint iClientID)
{
	mobiledockClients[iClientID].iDockingModulesInstalled = 0;
	list<EquipDesc> equipment = Players[iClientID].equipDescList.equip;
	for (list<EquipDesc>::iterator item = equipment.begin(); item != equipment.end(); item++)
	{
		if (item->iArchID == dockingModuleEquipmentID && item->bMounted)
		{
			mobiledockClients[iClientID].iDockingModulesInstalled++;
		}
	}

	return mobiledockClients[iClientID].iDockingModulesInstalled;
}

// Serializes info of specific client.
void SaveInfo(uint iClientID)
{
	// Is the disconnecting user a part of the docking module plugin at the moment?
	if (mobiledockClients.find(iClientID) != mobiledockClients.end())
	{
		wstring charName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID));

		// Is this a carrier?
		if (!mobiledockClients[iClientID].dockedShips.empty())
		{
			SaveDockInfoCarrier(charName, iClientID, mobiledockClients[iClientID]);
		}

		// Is this a carried ship?
		else if (!empty(mobiledockClients[iClientID].wscDockedWithCharname))
		{
			SaveDockInfoCarried(charName, iClientID, mobiledockClients[iClientID]);
		}

		mapPendingDockingRequests.erase(iClientID);
		mobiledockClients.erase(iClientID);
	}
}

// Checks if file exists.
inline bool CheckIfExists(const string& path)
{
	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Load the configuration
void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	// The path to the data file.
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	// Create directories if they don't exist.
	string moddir = string(datapath) + R"(\Accts\MultiPlayer\docking_module\)";
	CreateDirectoryA(moddir.c_str(), 0);

	char carrierlistpath[MAX_PATH];
	GetUserDataPath(carrierlistpath);
	moddir = string(datapath) + R"(\Accts\MultiPlayer\docking_module\carriers\)";
	CreateDirectoryA(moddir.c_str(), 0);

	char dockedlistpath[MAX_PATH];
	GetUserDataPath(dockedlistpath);
	moddir = string(dockedlistpath) + R"(\Accts\MultiPlayer\docking_module\dockedships\)";
	CreateDirectoryA(moddir.c_str(), 0);

	// Plugin configuration
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\dockingmodules.cfg";
	string scAmmolimitCfgFile = string(szCurDir) + "\\flhook_plugins\\ammolimits.cfg";
	string scAutobuyCfgFile = string(szCurDir) + "\\flhook_plugins\\autobuy.cfg";
	
	INI_Reader ini;
	// Read dock config file
	if(ini.open(scPluginCfgFile.c_str(), false))
	{
		while(ini.read_header())
		{
			if(ini.is_header("Config"))
			{
				while(ini.read_value())
				{
					if(ini.is_value("cargo_capacity_limit"))
					{
						cargoCapacityLimit = ini.get_value_int(0);
					}
					else if (ini.is_value("dock_distance"))
					{
						dockDistance = ini.get_value_float(0);
					}
					else if (ini.is_value("undock_distance"))
					{
						undockDistance = ini.get_value_int(0);
					}
					else if (ini.is_value("jettison_kick_time"))
					{
						jettisonKickTime = ini.get_value_int(0);
					}
					else if (ini.is_value("group_dock_delay"))
					{
						groupDockDelay = ini.get_value_int(0);
					}
					else if (ini.is_value("crew_minimum_limit"))
					{
						crewMinimum = ini.get_value_int(0);
					}
					else if (ini.is_value("crew_efficiency_const"))
					{
						crewEfficienyConst = ini.get_value_int(0);
					}
					else if (ini.is_value("crew_item"))
					{
						crewGoodID = CreateID(ini.get_value_string());
					}
					else if (ini.is_value("allowedmodule"))
					{
						dockingModuleEquipmentID = CreateID(ini.get_value_string());
					}
					else if (ini.is_value("supply_item"))
					{
						SUPPLY stats;
						string rawString = ini.get_value_string();
						uint itemID = CreateID(Trim(GetParam(rawString, '|', 0)).c_str());
						stats.efficiency = atoi(Trim(GetParam(rawString, '|', 1)).c_str());
						stats.type = atoi(Trim(GetParam(rawString, '|', 2)).c_str());
						mapSupplies[itemID] = stats;
					}
				}
			}
		}
		ini.close();
	}

	// Read ammolimit config file
	if (ini.open(scAmmolimitCfgFile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("ammolimit"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("limit"))
					{
						AMMO ammo;
						string name = ini.get_value_string(0);
						uint gunID;
						if (name.substr(name.size() - 5, 5) == "_ammo")
						{
							gunID = CreateID(name.substr(0, name.size() - 5).c_str());
							ammo.ammoID = CreateID(name.c_str());
						}
						else
						{
							gunID = CreateID(name.c_str());
							ammo.ammoID = CreateID((name + "_ammo").c_str());
						}
						ammo.ammoLimit = ini.get_value_int(1);
						mapAmmo[gunID] = ammo;
					}
				}
			}
		}
		ini.close();

		// If ammo limit is 1000+ - increase efficiency for these items.
		for (map<uint, AMMO>::iterator iter = mapAmmo.begin(); iter != mapAmmo.end(); iter++)
		if (iter->second.ammoLimit >= 1000)
		{
			boostedAmmo.push_back(iter->second.ammoID);
		}
	}

	// Read autobuy config file
	if (ini.open(scAutobuyCfgFile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("extra"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("item"))
					{
						uint itemID = CreateID(ini.get_value_string(0));
						uint ammoID = CreateID(ini.get_value_string(1));
						mapBatteries[itemID] = ammoID;
					}
				}
			}
			else if (ini.is_header("stackable"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("weapon"))
					{
						uint gunID = CreateID(ini.get_value_string(0));
						if (mapAmmo.find(gunID) != mapAmmo.end())
							mapAmmo[gunID].stackable = true;
					}
				}
			}
		}
		ini.close();
	}

	auto players = HkGetPlayers();
	for_each(players.begin(), players.end(), [](HKPLAYERINFO &player) { UpdateAvailableModules(player.iClientID); });

	string report = "DOCK: ";

	if (dockingModuleEquipmentID)
	{
		report += "Found docking module item. ";
	}
	else
	{
		ConPrint(L"DOCK: Not found docking module equipment item name in config file. Plugin disabled.\n");
		return;
	}

	if(crewGoodID)
	{
		report += "Found crew item. ";
	}
	else
	{
		report += "Crew item was not found in config file. Resupplying system disabled.\n";
		ConPrint(stows(report));
		return;
	}

	if (mapSupplies.size() > 0)
	{
		report += "Loaded " + to_string(mapSupplies.size()) + " supply items.\n";
	}
	else
	{
		report += "No supply items were found in config file. Resupplying system disabled.\n";
	}

	ConPrint(stows(report));
}


void __stdcall CharacterSelect(struct CHARACTER_ID const & cId, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	// Attempt to load the data for this ship if it exists. Trim off the .fl at the end of the file name.
	string charFilename = cId.szCharFilename;
	charFilename = charFilename.substr(0, charFilename.size() - 3);

	SaveInfo(iClientID);
	LoadShip(charFilename, iClientID);
}

void __stdcall CharacterSelect_AFTER(struct CHARACTER_ID const & cId, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	UpdateAvailableModules(iClientID);
}


void __stdcall BaseExit(uint iBaseID, uint iClientID)
{
	PrintUserCmdText(iClientID, L"BaseExit");
	returncode = DEFAULT_RETURNCODE;

	// Update count of available modules for the client.
	UpdateAvailableModules(iClientID);

	// If this is a ship which is currently docked, clear the market.
	if(mobiledockClients[iClientID].proxyBaseID)
	{
		SendResetMarketOverride(iClientID);
	}

	// If ship has requested /dockatbase sooner - cancel request.
	if (mapPendingDockingRequests.find(iClientID) != mapPendingDockingRequests.end())
	{
		mapPendingDockingRequests.erase(iClientID);
	}
}

void __stdcall PlayerLaunch(unsigned int iShip, unsigned int client)
{
	PrintUserCmdText(client, L"PlayerLaunch");
	returncode = DEFAULT_RETURNCODE;

	uint carrier_client = HkGetClientIdFromCharname(mobiledockClients[client].wscDockedWithCharname);

	wstring clientName = (const wchar_t*)Players.GetActiveCharacterName(client);

	// Check if ship is carried.
	if (mobiledockClients[client].proxyBaseID && mobiledockClients[client].dockedShips.empty() && !mobiledockClients[client].wscDockedWithCharname.empty())
	{
		// Set last real base from imaginary to real base extracted from list.
		Players[client].iLastBaseID = mobiledockClients[client].iLastBaseID;

		// If carrier is not at server now.
		if (carrier_client == -1)
		{
			SHIP_LOCATION location = GetCarrierPosOffline(client);

			// If failed to read data - return.
			if (!location.baseID && !location.systemID)
				return;
			// If carrier at base, load its last base.
			if (location.baseID)
			{
				mobiledockClients[client].iLastBaseID = location.baseID;
				mobiledockClients[client].proxyBaseID = -1;
				return;
			}
			// If carrier in space, load its last known position.
			else
			{
				mobiledockClients[client].carrierSystem = location.systemID;
				mobiledockClients[client].carrierPos = location.pos;
				mobiledockClients[client].carrierRot = EulerMatrix(location.rot);
			}

			// Check if carrier gone offline in space.
			if (!location.baseID)
			{
				// Check if carrier and docked ship are in same system.
				if (mobiledockClients[client].carrierSystem == Players[client].iSystemID)
				{
					// Push docked ship to undocking queue.
					undockingQueue.push_back(mobiledockClients[client]);

					// Remove client from list, because it is in undocking queue.
					mobiledockClients.erase(client);
				}
				else
				{
					// Redirect to proxy base in carrier system.
					uint iBaseID = GetProxyBaseForSystem(mobiledockClients[client].carrierSystem);

					mobiledockClients[client].proxyBaseID = iBaseID;
					mobiledockClients[client].carrierSystem = -1;
				}
			}
			return;
		}

		// Get the carrier ship information.
		uint carrierShip;
		pub::Player::GetShip(carrier_client, carrierShip);

		uint clientShip;
		pub::Player::GetShip(client, clientShip);

		// Check to see if the carrier is currently in a base. If so, force the client to dock on that base.
		if(!carrierShip)
		{

			uint iBaseID;
			pub::Player::GetBase(carrier_client, iBaseID);

			if(!iBaseID)
			{
				return;
			}

			// If carrier at POB now, get last known carrier position in space, because plugin can't undock ship directly to POB.
			if (iBaseID == GetProxyBaseForClient(carrier_client))
			{
				mobiledockClients[client].carrierPos = mobiledockClients[carrier_client].carrierPos;
				mobiledockClients[client].carrierRot = mobiledockClients[carrier_client].carrierRot;
				mobiledockClients[client].carrierSystem = mobiledockClients[carrier_client].carrierSystem;

				if (Players[client].iSystemID == Players[carrier_client].iSystemID)
				{
					undockingQueue.push_back(mobiledockClients[client]);

					mobiledockClients.erase(client);

					vector<wstring> &dockedShips = mobiledockClients[carrier_client].dockedShips;
					dockedShips.erase(std::remove(dockedShips.begin(), dockedShips.end(), clientName), dockedShips.end());

					pub::Audio::PlaySoundEffect(carrier_client, CreateID("cargo_jettison"));
				}
				else
				{
					// Redirect to proxy base in carrier system.
					mobiledockClients[client].proxyBaseID = GetProxyBaseForClient(carrier_client);
					mobiledockClients[client].carrierSystem = -1;
				}

				return;
			}

			// Set proxyBaseID to -1 to allow PlayerLaunch_AFTER handle undocking.
			mobiledockClients[client].proxyBaseID = -1;

			// Remove ship from carrier list of docked ships.
			vector<wstring> &dockedShips = mobiledockClients[carrier_client].dockedShips;
			dockedShips.erase(std::remove(dockedShips.begin(), dockedShips.end(), clientName), dockedShips.end());

			pub::Audio::PlaySoundEffect(carrier_client, CreateID("cargo_jettison"));

			return;
		}

		// Check if carrier and docked ship are in same system.
		if (Players[client].iSystemID == Players[carrier_client].iSystemID)
		{
			// Update the internal values of the docked ship pretaining to the carrier.
			UpdateCarrierLocationInformation(client, carrierShip);

			undockingQueue.push_back(mobiledockClients[client]);

			mobiledockClients.erase(client);

			vector<wstring> &dockedShips = mobiledockClients[carrier_client].dockedShips;
			dockedShips.erase(std::remove(dockedShips.begin(), dockedShips.end(), clientName), dockedShips.end());

			pub::Audio::PlaySoundEffect(carrier_client, CreateID("cargo_jettison"));
		}
		else
		{
			// Redirect to proxy base in carrier system.
			mobiledockClients[client].proxyBaseID = GetProxyBaseForClient(carrier_client);
			mobiledockClients[client].carrierSystem = -1;
		}
		return;
	}

	// Check if ship tries to escape from dying carrier.
	if (mobiledockClients[client].proxyBaseID && mobiledockClients[client].proxyBaseID != -1 && mobiledockClients[client].dockedShips.empty() && mobiledockClients[client].wscDockedWithCharname.empty())
	{
		// Set last real base from imaginary to real base extracted from list.
		Players[client].iLastBaseID = mobiledockClients[client].iLastBaseID;

		// Check if carrier and docked ship are in same system.
		if (Players[client].iSystemID == mobiledockClients[client].carrierSystem)
		{
			undockingQueue.push_back(mobiledockClients[client]);
			mobiledockClients.erase(client);
		}
		else
		{
			// Redirect to proxy base in carrier system.
			mobiledockClients[client].proxyBaseID = GetProxyBaseForSystem(mobiledockClients[client].carrierSystem);
			mobiledockClients[client].carrierSystem = -1;
		}
	}
}

bool __stdcall LaunchPosHook(uint space_obj, struct CEqObj &p1, Vector &pos, Matrix &rot, int dock_mode)
{
	returncode = DEFAULT_RETURNCODE;

	for (vector<CLIENT_DATA>::iterator it = undockingQueue.begin(); it != undockingQueue.end(); it++)
		if (it->proxyBaseID == space_obj)
		{
			returncode = SKIPPLUGINS_NOFUNCTIONCALL;

			rot = it->carrierRot;
			pos = GetUndockingPosition(it->carrierPos, rot);
			undockingQueue.erase(it);
		}
	return true;
}

void __stdcall PlayerLaunch_AFTER(unsigned int iShip, unsigned int client)
{
	PrintUserCmdText(client, L"PlayerLaunch_AFTER");
	returncode = DEFAULT_RETURNCODE;

	// If undocking from carrier in base, land ship to its base.
	if (mobiledockClients[client].proxyBaseID == -1)
	{
		pub::Player::ForceLand(client, mobiledockClients[client].iLastBaseID);	
		mobiledockClients.erase(client);
		return;
	}

	// If carrier is in another system when undocking, swap systems.
	if (mobiledockClients[client].carrierSystem == -1)
	{
		PrintUserCmdText(client, L"Navigational map was successfully updated.");
		pub::Player::ForceLand(client, mobiledockClients[client].proxyBaseID);
		mobiledockClients[client].carrierSystem = Players[client].iSystemID;
		return;
	}

	// If this is base-docking, land ship to carrier.
	if (mobiledockClients[client].iLastBaseID == -1 &&
		Players[client].iLastBaseID == Players[HkGetClientIdFromCharname(mobiledockClients[client].wscDockedWithCharname)].iBaseID &&
		UpdateAvailableModules(HkGetClientIdFromCharname(mobiledockClients[client].wscDockedWithCharname)) - mobiledockClients[HkGetClientIdFromCharname(mobiledockClients[client].wscDockedWithCharname)].dockedShips.size() != 0 &&
		(Archetype::GetShip(Players[client].iShipArchetype))->fHoldSize <= cargoCapacityLimit)
	{
		uint iTargetClientID = HkGetClientIdFromCharname(mobiledockClients[client].wscDockedWithCharname);

		DockShip(iShip, iTargetClientID, client);
		return;
	}
}


int __cdecl Dock_Call(unsigned int const &iShip, unsigned int const &iBaseID, int iCancel, enum DOCK_HOST_RESPONSE response)
{
	returncode = DEFAULT_RETURNCODE;

	UINT client = HkGetClientIDByShip(iShip);
	PrintUserCmdText(client, L"DockCall");

	// If carrier docks to POB, update its last location, because I can't make this plugin warp players directly to POBs.
	if (client && mobiledockClients[client].iDockingModulesInstalled > 0)
	{
		pub::SpaceObj::GetLocation(iShip, mobiledockClients[client].carrierPos, mobiledockClients[client].carrierRot);
		mobiledockClients[client].carrierSystem = Players[client].iSystemID;
	}

	// If it is docking request from player ship - then process it.
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

		// If target is not player ship - ignore request.
		const uint iTargetClientID = HkGetClientIDByShip(iTargetShip);
		if (!iTargetClientID)
			return 0;

		// If ships are too far each from other, notify player about it.
		if (HkDistance3DByShip(iShip, iTargetShip) > dockDistance)
		{
			PrintUserCmdText(client, L"Ship is out of range");
			return 0;
		}

		// Check that the target ship has an empty docking module.
		if (mobiledockClients[iTargetClientID].iDockingModulesInstalled - mobiledockClients[iTargetClientID].dockedShips.size() == 0)
		{
			PrintUserCmdText(client, L"Target ship has no free docking capacity");
			return 0;
		}

		// Check that the requesting ship is of the appropriate size to dock.
		Archetype::Ship *ship = Archetype::GetShip(Players[client].iShipArchetype);
		if (ship->fHoldSize > cargoCapacityLimit)
		{
			PrintUserCmdText(client, L"Target ship cannot dock a ship of your size.");
			return 0;
		}

		CShip* cship = dynamic_cast<CShip*>(HkGetEqObjFromObjRW(reinterpret_cast<IObjRW*>(HkGetInspect(iTargetClientID))));
		if (cship->get_hit_pts() <= 0.0f)
		{
			PrintUserCmdText(client, L"You cannot dock to exploding ship.");
			return 0;
		}

		// Check if requesting ship is in group with carrier.
		bool IsInGroupWithCarrier = false;
		pub::Player::IsGroupMember(client, iTargetClientID, IsInGroupWithCarrier);

		if (IsInGroupWithCarrier)
		{
			// Create a docking request to make docking cancelable.
			mapPendingDockingRequests[client] = iTargetClientID;
			PrintUserCmdText(client, L"Wait. Docking in %i seconds.", groupDockDelay);

			TaskScheduler.push_back(Task(groupDockDelay, [client, iTargetClientID, iShip, iTargetShip]()
			{
				// Check if ship still requesting dock.
				if (mapPendingDockingRequests.find(client) == mapPendingDockingRequests.end()) 
					return;

				// Check if carrier ship is still not gone.
				if (HkDistance3DByShip(iShip, iTargetShip) > dockDistance)
				{
					PrintUserCmdText(client, L"Ship is out of range");
					return;
				}

				// Check that the target ship still has empty docking module.
				if (mobiledockClients[iTargetClientID].iDockingModulesInstalled - mobiledockClients[iTargetClientID].dockedShips.size() == 0)
				{
					PrintUserCmdText(client, L"Target ship has no free docking capacity");
					return;
				}

				CShip* carriership = dynamic_cast<CShip*>(HkGetEqObjFromObjRW(reinterpret_cast<IObjRW*>(HkGetInspect(iTargetClientID))));
				if (carriership->get_hit_pts() <= 0.0f)
				{
					PrintUserCmdText(client, L"You cannot dock to exploding ship.");
					return;
				}

				DockShip(iTargetShip, iTargetClientID, client);
				return;
			}));

			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
			return -1;
		}
		// Create a docking request and send a notification to the target ship.
		mapPendingDockingRequests[client] = iTargetClientID;
		PrintUserCmdText(client, L"Docking request sent to %s. Player must authorize you with /allowdock command or you must be in same group.", Players.GetActiveCharacterName(iTargetClientID));
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
		return -1;
	}
	return 0;
}

void __stdcall RequestCancel(int iType, unsigned int iShip, unsigned int p3, unsigned long p4, unsigned int iClientID)
{
	PrintUserCmdText(iClientID, L"RequestCancel");
	returncode = DEFAULT_RETURNCODE;

	// If ship requested dock sooner - cancel request
	if (mapPendingDockingRequests.find(iClientID) != mapPendingDockingRequests.end())
	{
		mapPendingDockingRequests.erase(iClientID);
	}
}


void __stdcall BaseEnter(uint iBaseID, uint client)
{
	PrintUserCmdText(client, L"BaseEnter");
	returncode = DEFAULT_RETURNCODE;

	if (mobiledockClients[client].proxyBaseID)
	{
		// Clear the market. We don't support transfers in this version.
		SendResetMarketOverride(client);

		// Set the base name.
		wstring status = L"<RDL><PUSH/>";
		status += L"<TEXT>" + XMLText(mobiledockClients[client].wscDockedWithCharname) + L"</TEXT><PARA/><PARA/>";
		status += L"<POP/></RDL>";
		SendSetBaseInfoText2(client, status);
	}
	else if(mobiledockClients[client].iDockingModulesInstalled > 0)
	{
		// Set last base for carrier.
		mobiledockClients[client].iLastBaseID = iBaseID;
	}

	// Change base for docked ships if base for carrier changes.
	vector<wstring> &dockedShips = mobiledockClients[client].dockedShips;
	if (!dockedShips.empty() && iBaseID != GetProxyBaseForClient(client))
	{
		for (vector<wstring>::iterator i = dockedShips.begin(); i != dockedShips.end(); ++i)
		{
			uint iDockedClientID = HkGetClientIdFromCharname(*i);
			if (iDockedClientID != -1)
			{
				mobiledockClients[iDockedClientID].iLastBaseID = mobiledockClients[client].iLastBaseID;
			}
			else
			{
				UpdateLastBaseOffline(*i, mobiledockClients[client].iLastBaseID);
			}
		}
	}

	// If ship requested dock sooner - cancel request
	if (mapPendingDockingRequests.find(client) != mapPendingDockingRequests.end())
	{
		mapPendingDockingRequests.erase(client);
	}
}

void __stdcall ShipDestroyed(DamageList *_dmg, DWORD *ecx, uint kill)
{
	returncode = DEFAULT_RETURNCODE;

	CShip *cship = (CShip*)ecx[4];
	uint client = cship->GetOwnerPlayer();
	Vector shipPosition = cship->get_position();
	Matrix shipOrientation = cship->get_orientation();

	if (kill)
	{
		if (client)
		{
			// If this is a carrier then drop all docked ships into space.
			if (!mobiledockClients[client].dockedShips.empty())
			{
				wstring carrierCharname = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(client));
				uint system = mobiledockClients[client].carrierSystem;
				vector<wstring> dockedShips = mobiledockClients[client].dockedShips;

				// Send a system switch to force each ship to launch.
				for (vector<wstring>::iterator i = mobiledockClients[client].dockedShips.begin();
					i != mobiledockClients[client].dockedShips.end(); ++i)
				{
					uint iDockedClientID = HkGetClientIdFromCharname(*i);
					uint lastBaseID = 0;

					if (iDockedClientID != -1)
					{
						// Update the coordinates the given ship should launch to.
						UpdateCarrierLocationInformation(iDockedClientID, shipPosition, shipOrientation);
						mobiledockClients[iDockedClientID].carrierSystem = mobiledockClients[client].carrierSystem;

						// Clear this field, because the carrier is no more.
						mobiledockClients[iDockedClientID].wscDockedWithCharname.clear();

						PrintUserCmdText(iDockedClientID, L"ALERT! Carrier ship is being destroyed. Launch ship or you will die in explosion.");
						pub::Audio::PlaySoundEffect(iDockedClientID, CreateID("rtc_klaxon_loop"));
						lastBaseID = mobiledockClients[iDockedClientID].iLastBaseID;
					}
					else
					{
						UpdateDyingCarrierPos(*i, shipPosition, shipOrientation, system);
					}

					wstring dockedCharname = *i;

					TaskScheduler.push_back(Task(jettisonKickTime, [dockedShips, dockedCharname, carrierCharname, shipPosition, system, lastBaseID]()
					{
						uint checkDockedClientID = HkGetClientIdFromCharname(dockedCharname);

						// If docked ship is at server - kick it and throw its cargo in space, if not - just drop cargo.
						if (checkDockedClientID != -1)
						{
							if (mobiledockClients[checkDockedClientID].proxyBaseID && (mobiledockClients[checkDockedClientID].wscDockedWithCharname.empty() || mobiledockClients[checkDockedClientID].wscDockedWithCharname == carrierCharname) )
							{
								HkKick(HkGetAccountByClientID(checkDockedClientID));
								ThrowCargoOffline(dockedCharname, shipPosition, system, lastBaseID);
							}
						}
						else
						{
							ThrowCargoOffline(dockedCharname, shipPosition, system, 0);
						}
					}));
				}

				// Clear the carrier from the list.
				mobiledockClients[client].dockedShips.clear();
			}
		}
	}
}

void __stdcall SystemSwitchOutComplete_AFTER(unsigned int iShipID, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	// After completing a jump, if it's in our dockedships map, we update the current system.
	if (mobiledockClients.find(iClientID) != mobiledockClients.end())
	{
		// Get system, to which carrier jumped.
		uint systemJumpedTo;
		pub::SpaceObj::GetSystem(iShipID, systemJumpedTo);

		// If it is same system - do nothing.
		if (mobiledockClients[iClientID].carrierSystem != systemJumpedTo)
		{
			// Update carrier system.
			mobiledockClients[iClientID].carrierSystem = systemJumpedTo;

			// Notify all docked ships about system switch.
			for (vector<wstring>::iterator i = mobiledockClients[iClientID].dockedShips.begin();
				i != mobiledockClients[iClientID].dockedShips.end(); ++i)
			{
				wstring charname = *i;
				uint iDockedClientID = HkGetClientIdFromCharname(charname);
				if (iDockedClientID != -1)
				{
					pub::Audio::PlaySoundEffect(iDockedClientID, CreateID("jump_in"));
				}

				TaskScheduler.push_back(Task(2, [charname]()
				{
					uint dockedClientID = HkGetClientIdFromCharname(charname);
					PrintUserCmdText(dockedClientID, L"Carrier ship jumped to another system. Launch your ship to update navmap");
				}));
			}
		}
	}
}

void __stdcall SPScanCargo(unsigned int const &p1, unsigned int const &p2, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	uint iTargetShip;
	pub::SpaceObj::GetTarget(Players[iClientID].iShipID, iTargetShip);
	if (!iTargetShip)
		return;

	uint iType;
	pub::SpaceObj::GetType(iTargetShip, iType);
	if (iType != OBJ_FREIGHTER)
		return;

	// If target is not player ship - ignore request.
	const uint iTargetClientID = HkGetClientIDByShip(iTargetShip);
	if (!iTargetClientID)
		return;

	if (mobiledockClients[iTargetClientID].iDockingModulesInstalled > 0)
	{
		PrintUserCmdText(iClientID, EnumerateDockedShips(iTargetClientID));
	}
}

void __stdcall SystemSwitchOutComplete(unsigned int iShipID, unsigned int iClientID)
{
	PrintUserCmdText(iClientID, L"SystemSwitchOutComplete");
	returncode = DEFAULT_RETURNCODE;
}

void __stdcall JumpInComplete(uint iSystemID, uint iShip)
{
	UINT client = HkGetClientIDByShip(iShip);
	PrintUserCmdText(client, L"JumpInComplete");
	returncode = DEFAULT_RETURNCODE;
}

// The user chat commands for this plugin
USERCMD UserCmds[] =
{
	{ L"/listdocked",		CMD_listdocked		 },
	{ L"/conn",				CMD_conn			 },
	{ L"/return",			CMD_return			 },
	{ L"/renameme",			CMD_renameme		 },
	{ L"/jettisonship",		CMD_jettisonship	 },
	{ L"/jettisonallships",	CMD_jettisonallships },
	{ L"/allowdock",		CMD_allowdock		 },
	{ L"/dockatbase",		CMD_dockatbase		 },
	{ L"/loadsupplies",		CMD_loadsupplies	 },
};

bool UserCmd_Process(uint iClientID, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	for (uint i = 0; (i < sizeof(UserCmds) / sizeof(USERCMD)); i++)
	{
		if (wscCmd.find(UserCmds[i].wszCmd) == 0)
		{
			if (!UserCmds[i].proc(iClientID, wscCmd))
			{
				returncode = SKIPPLUGINS; // If associated function returns 'false', prevent this command from being handled by other plugins.
			}

			return true;
		}
	}
	return false;
}


void __stdcall DisConnect(uint iClientID, enum EFLConnection p2)
{
	returncode = DEFAULT_RETURNCODE;
	SaveInfo(iClientID);
}

void __stdcall DestroyCharacter(struct CHARACTER_ID const &cId, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	// Is this character part of dock plugin now?
	if (mobiledockClients.find(iClientID) != mobiledockClients.end())
	{
		wstring charname = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID));

		// Is this docked ship?
		if (!mobiledockClients[iClientID].wscDockedWithCharname.empty())
		{
			uint carrierClientID = HkGetClientIdFromCharname(mobiledockClients[iClientID].wscDockedWithCharname);

			if (carrierClientID != -1)
			{
				vector<wstring> &dockedShips = mobiledockClients[carrierClientID].dockedShips;
				dockedShips.erase(std::remove(dockedShips.begin(), dockedShips.end(), charname), dockedShips.end());
			}
			else
			{
				// We don't need position. This function also removes character from carrier dockedchars list.
				GetCarrierPosOffline(0, charname);
			}
		}
		// Is this carrier ship?
		else if (!mobiledockClients[iClientID].dockedShips.empty())
		{
			for (uint i = 0; i < mobiledockClients[iClientID].dockedShips.size(); i++)
			{
				uint dockedClientID = HkGetClientIdFromCharname(mobiledockClients[iClientID].dockedShips[i]);

				// If online, redirect it to last carrier position.
				if (dockedClientID != -1)
				{
					mobiledockClients[dockedClientID].wscDockedWithCharname.clear();

					// If this is POB, move ship to last known carrier coordinates in space.
					if (mobiledockClients[iClientID].iLastBaseID == GetProxyBaseForSystem(mobiledockClients[iClientID].carrierSystem))
					{
						mobiledockClients[dockedClientID].carrierPos = mobiledockClients[iClientID].carrierPos;
						mobiledockClients[dockedClientID].carrierRot = mobiledockClients[iClientID].carrierRot;
						mobiledockClients[dockedClientID].carrierSystem = mobiledockClients[iClientID].carrierSystem;
					}
					else
					{
						mobiledockClients[dockedClientID].proxyBaseID = -1;
					}
				}
				// If not, jettison offline.
				else
				{
					JettisonShipOffline(mobiledockClients[iClientID].dockedShips[i], charname);
				}
			}
		}
	}
	// If this client has not logged any character before removing one, things become harder.
	else
	{
		CAccount* acc = HkGetAccountByClientID(iClientID);
		wstring charname = HkGetCharnameFromCharFile(cId.szCharFilename, acc);
		bool foundFile;

		// Try loading this ship as docked.
		string path = GetSavePath(charname, false);
		foundFile = CheckIfExists(path);

		if (foundFile)
		{
			map<string, vector<string>> fields;
			fields["dockedwith"].push_back("=");
			ReadFLFile(fields, path);

			// Delete docked ship save file.
			_unlink(path.c_str());

			// Then remove ship from carrier docked ships list.
			wstring carrierCharname = stows(fields["dockedwith"][1]);
			string carrierPath = GetSavePath(carrierCharname, true);
			bool foundCarrierFile = CheckIfExists(carrierPath);

			// If file was not found, then carrier ship is at server now.
			if (!foundCarrierFile)
			{
				uint carrierClientID = HkGetClientIdFromCharname(carrierCharname);
				vector<wstring> &dockedShips = mobiledockClients[carrierClientID].dockedShips;
				dockedShips.erase(std::remove(dockedShips.begin(), dockedShips.end(), charname), dockedShips.end());
			}
			else
			{
				vector<string> linesToDelete = { "dockedchar=" + wstos(charname) };
				EditFLFile(&linesToDelete, nullptr, nullptr, carrierPath, false, true);
			}

			return;
		}

		// Try loading ship as carrier.
		path = GetSavePath(charname, true);
		foundFile = CheckIfExists(path);

		if (foundFile)
		{
			map<string, vector<string>> fields;
			fields["dockedchar"].push_back("=");
			ReadFLFile(fields, path);

			// Undock all docked ships.
			for (uint i = 1; i < fields["dockedchar"].size(); i++)
			{
				wstring dockedCharname = stows(fields["dockedchar"][i]);
				string dockedPath = GetSavePath(dockedCharname, false);				
				bool foundDockedFile = CheckIfExists(dockedPath);

				// If file was not found, then docked ship is at server now.
				if (!foundDockedFile)
				{
					uint dockedClientID = HkGetClientIdFromCharname(dockedCharname);

					SHIP_LOCATION location = GetCarrierPosOffline(dockedClientID);
					mobiledockClients[dockedClientID].wscDockedWithCharname.clear();

					// If failed to read data - return.
					if (!location.baseID && !location.systemID)
						return;
					if (location.baseID)
						mobiledockClients[dockedClientID].proxyBaseID = -1;
					// If not, redirect to last known position in space.
					else
					{
						UpdateCarrierLocationInformation(dockedClientID, location.pos, EulerMatrix(location.rot));
						mobiledockClients[dockedClientID].carrierSystem = location.systemID;
					}
				}
				else
				{
					JettisonShipOffline(dockedCharname, charname);
				}
			}
		}
	}
}


void __stdcall GFGoodSell(struct SGFGoodSellInfo const &gsi, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	if (gsi.iArchID == dockingModuleEquipmentID)
	{
		if (UpdateAvailableModules(iClientID) - 1 < mobiledockClients[iClientID].dockedShips.size())
		{
			wstring dockedCharname = mobiledockClients[iClientID].dockedShips[0];
			vector<wstring> &dockedShips = mobiledockClients[iClientID].dockedShips;
			dockedShips.erase(std::remove(dockedShips.begin(), dockedShips.end(), dockedCharname), dockedShips.end());

			JettisonShip(iClientID, dockedCharname, false);
		}
	}
}

void __stdcall ReqEquipment(class EquipDescList const &edl, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	uint modules = 0;
	list<EquipDesc> equipment = edl.equip;
	for (list<EquipDesc>::iterator item = equipment.begin(); item != equipment.end(); item++)
	{
		if (item->iArchID == dockingModuleEquipmentID && item->bMounted)
		{
			modules++;
		}
	}

	if (modules < mobiledockClients[iClientID].dockedShips.size())
	{
		wstring dockedCharname = mobiledockClients[iClientID].dockedShips[0];
		vector<wstring> &dockedShips = mobiledockClients[iClientID].dockedShips;
		dockedShips.erase(std::remove(dockedShips.begin(), dockedShips.end(), dockedCharname), dockedShips.end());

		JettisonShip(iClientID, dockedCharname, false);
	}
}

void HkTimerCheckKick()
{
	returncode = DEFAULT_RETURNCODE;
	for (vector<Task>::iterator it = TaskScheduler.begin(); it != TaskScheduler.end(); it++)
	{
		if (chrono::system_clock::now() >= it->expiryTime)
		{
			it->Function();
			TaskScheduler.erase(it);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 3));

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Dock_Call, PLUGIN_HkCb_Dock_Call, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&RequestCancel, PLUGIN_HkIServerImpl_RequestCancel, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ShipDestroyed, PLUGIN_ShipDestroyed, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SystemSwitchOutComplete_AFTER, PLUGIN_HkIServerImpl_SystemSwitchOutComplete_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SystemSwitchOutComplete, PLUGIN_HkIServerImpl_SystemSwitchOutComplete, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&JumpInComplete, PLUGIN_HkIServerImpl_JumpInComplete_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SPScanCargo, PLUGIN_HkIServerImpl_SPScanCargo, 0));

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseEnter, PLUGIN_HkIServerImpl_BaseEnter, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseExit, PLUGIN_HkIServerImpl_BaseExit, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch, PLUGIN_HkIServerImpl_PlayerLaunch, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LaunchPosHook, PLUGIN_LaunchPosHook, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch_AFTER, PLUGIN_HkIServerImpl_PlayerLaunch_AFTER, 0));

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkTimerCheckKick, PLUGIN_HkTimerCheckKick, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterSelect, PLUGIN_HkIServerImpl_CharacterSelect, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterSelect_AFTER, PLUGIN_HkIServerImpl_CharacterSelect_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&DisConnect, PLUGIN_HkIServerImpl_DisConnect, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&DestroyCharacter, PLUGIN_HkIServerImpl_DestroyCharacter, 0));

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&GFGoodSell, PLUGIN_HkIServerImpl_GFGoodSell, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ReqEquipment, PLUGIN_HkIServerImpl_ReqEquipment, 0));

	return p_PI;
}
