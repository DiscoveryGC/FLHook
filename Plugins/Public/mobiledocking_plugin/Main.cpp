/**
 * Mobile Docking Plugin for FLHook
 * Initial Version by Cannon
 * Using some work written by Alley
 * Rework by Remnant
 */

#include "Main.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <ctime>

PLUGIN_RETURNCODE returncode;
unordered_map<uint, uint> mapPendingDockingRequests;
unordered_map<uint, DELAYEDDOCK> dockingInProgress;

unordered_map<uint, uint> dockingModuleEquipmentCapacityMap;
unordered_map<uint, CLIENT_DATA> mobiledockClients;

unordered_map<wstring, CARRIERINFO> nameToCarrierInfoMap;
unordered_map<wstring, DOCKEDCRAFTINFO> nameToDockedInfoMap;

unordered_map<uint, CARRIERINFO*> idToCarrierInfoMap;
unordered_map<uint, DOCKEDCRAFTINFO*> idToDockedInfoMap;

unordered_map<uint,uint> jettisonedShipsQueue;

unordered_set<uint> bannedSystems;

std::mutex saveMutex;
std::thread saveThread;

enum JettisonResult
{
	Success,
	InvalidName,
	ShipInSpace
};

uint forgetCarrierDataInSeconds = 31556926; // since it's all kept in a single file, clear old enough values. Default value being a year

// By default, docking is instant, but can be set to take defined number of seconds.
uint dockingPeriod = 0;

// Above how much cargo capacity, should a ship be rejected as a docking user?
int cargoCapacityLimit = 275;

float mobileDockingRange = 500.0f;

float maxDockingDistanceTolerance = 8.0f;

string defaultReturnSystem = "limbo"; //freeport 9, Omicron Theta
string defaultReturnBase = "limbo_01_base";

uint mobileDockingProxyBase = CreateID("limbo_01_base");
bool disableShieldsOnDockAttempt = true;
bool disableRegensOnDockAttempt = true;

void MoveOfflineShipToLastDockedSolar(const wstring& charName);

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
					if (ini.is_value("module"))
					{
						dockingModuleEquipmentCapacityMap[CreateID(ini.get_value_string(0))] = ini.get_value_int(1);
						dockingModAmount++;
					}
					else if (ini.is_value("cargo_capacity_limit"))
					{
						cargoCapacityLimit = ini.get_value_int(0);
					}
					else if (ini.is_value("carrier_data_wipe_period"))
					{
						forgetCarrierDataInSeconds = ini.get_value_int(0);
					}
					else if (ini.is_value("mobile_dock_range"))
					{
						mobileDockingRange = ini.get_value_float(0);
					}
					else if (ini.is_value("docking_period"))
					{
						dockingPeriod = ini.get_value_int(0);
					}
					else if (ini.is_value("docking_tolerance"))
					{
						maxDockingDistanceTolerance = ini.get_value_float(0);
					}
					else if (ini.is_value("default_return_system"))
					{
						defaultReturnSystem = ini.get_value_string(0);
					}
					else if (ini.is_value("default_return_base"))
					{
						defaultReturnBase = ini.get_value_string(0);
					}
					else if (ini.is_value("target_mobile_docking_base"))
					{
						mobileDockingProxyBase = CreateID(ini.get_value_string(0));
					}
					else if (ini.is_value("disable_docking_repairs"))
					{
						disableRegensOnDockAttempt = ini.get_value_bool(0);
					}
					else if (ini.is_value("docking_shield_drain"))
					{
						disableShieldsOnDockAttempt = ini.get_value_bool(0);
					}
					else if (ini.is_value("banned_system"))
					{
						bannedSystems.insert(CreateID(ini.get_value_string(0)));
					}
				}
			}
		}
		ini.close();
	}

	if (ini.open(scCarrierDataFile.c_str(), false))
	{
		std::lock_guard<std::mutex> saveLock(saveMutex);
		time_t curTime = time(0);
		while (ini.read_header())
		{
			if (ini.is_header("CarrierData"))
			{
				CARRIERINFO ci;
				wstring carrierName;
				boolean doLoad = true;
				while (ini.read_value())
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
							&& (curTime - forgetCarrierDataInSeconds > ini.get_value_int(0)))
						{
							doLoad = false;
						}
						else
						{
							ci.lastCarrierLogin = ini.get_value_int(0);
						}
					}
					else if (ini.is_value("docked"))
					{
						wstring& dockedShipName = stows(ini.get_value_string(0));
						const auto& accData = HkGetAccountByCharname(dockedShipName);
						// skip loading renamed/deleted docked ships
						if (!accData)
						{
							continue;
						}

						nameToDockedInfoMap[dockedShipName].carrierName = carrierName.c_str();
						nameToDockedInfoMap[dockedShipName].lastDockedSolar = CreateID(ini.get_value_string(1));

						ci.dockedShipList.emplace_back(dockedShipName);
						dockedCount++;
					}
				}
				if (doLoad)
				{
					nameToCarrierInfoMap[carrierName] = ci;
					carrierCount++;
				}
				else
				{
					for (const wstring& dockedShipName : ci.dockedShipList)
					{
						MoveOfflineShipToLastDockedSolar(dockedShipName);
					}
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
	try
	{
		while (true)
		{
			std::this_thread::sleep_for(std::chrono::minutes(1));

			if (nameToCarrierInfoMap.empty())
			{
				continue;
			}

			char datapath[MAX_PATH];
			GetUserDataPath(datapath);
			string path = string(datapath) + R"(\Accts\MultiPlayer\docking_module\mobile_docking.ini)";
			FILE* file = fopen(path.c_str(), "w");
			if (file)
			{
				std::lock_guard<std::mutex> saveLock(saveMutex);
				for (const auto& ci : nameToCarrierInfoMap)
				{
					if (ci.second.dockedShipList.empty())
					{
						continue;
					}
					fprintf(file, "[CarrierData]\n");
					fprintf(file, "carrier = %ls\n", ci.first.c_str());
					fprintf(file, "lastLogin = %u\n", (uint)ci.second.lastCarrierLogin);
					for (const wstring& dockedName : ci.second.dockedShipList)
					{
						const auto& lastSolarInfo = Universe::get_base(nameToDockedInfoMap[dockedName.c_str()].lastDockedSolar);
						if (lastSolarInfo)
						{
							fprintf(file, "docked = %ls, %s\n", dockedName.c_str(), lastSolarInfo->cNickname);
						}
					}
				}

				fclose(file);
			}
			else
			{
				ConPrint(L"ERROR MobileDock failed to open the player data file!\n");
				AddLog("ERROR MobileDock failed to open the player data file!\n");
			}
		}
	}
	catch (exception& e)
	{
		AddLog("ERROR MOBILEDOCK SAVE: %s", e.what());
		throw(e);
	}
}

wstring GetLastBaseName(uint client)
{
	const auto& dockedInfo = idToDockedInfoMap[client];
	const auto& baseInfo = Universe::get_base(dockedInfo->lastDockedSolar);
	const auto& sysInfo = Universe::get_system(baseInfo->iSystemID);
	wstring baseName;
	wstring& sysName = HkGetWStringFromIDS(sysInfo->strid_name);
	// if last base is a proxy base, it's a POB, call to base plugin for its name
	if (((string)baseInfo->cNickname).find("_proxy_base") != string::npos)
	{
		LAST_PLAYER_BASE_NAME_STRUCT pobName;
		pobName.clientID = client;
		Plugin_Communication(PLUGIN_MESSAGE::CUSTOM_BASE_LAST_DOCKED, &pobName);
		baseName = L"Player base " + pobName.lastBaseName;
	}
	else
	{
		baseName = HkGetWStringFromIDS(baseInfo->iBaseIDS);
	}
	return baseName + L", " + sysName;
}

JettisonResult RemoveShipFromLists(const wstring& dockedShipName, bool forcedLaunch)
{
	if (!nameToDockedInfoMap.count(dockedShipName))
	{
		return InvalidName;
	}
	uint dockedClientID = HkGetClientIdFromCharname(dockedShipName);

	if (dockedClientID == -1)
	{
		//player offline, edit their character file to put them on last docked solar
		MoveOfflineShipToLastDockedSolar(dockedShipName);
		return Success;
	}
	uint ship;
	pub::Player::GetShip(dockedClientID, ship);
	if (ship)
	{
		return ShipInSpace;
	}
	//Force launch the ship into space if it's on the carrier.
	if (forcedLaunch)
	{
		uint carrierId = HkGetClientIdFromCharname(idToDockedInfoMap[dockedClientID]->carrierName);
		jettisonedShipsQueue[dockedClientID] = carrierId;
		ForceLaunch(dockedClientID);

		wstring newBaseInfo = GetLastBaseName(dockedClientID);
		PrintUserCmdText(dockedClientID, L"You've been forcefully jettisoned by the carrier.");
		PrintUserCmdText(dockedClientID, L"Current home base: %ls", newBaseInfo.c_str());
	}
	Players[dockedClientID].iLastBaseID = idToDockedInfoMap[dockedClientID]->lastDockedSolar;
	idToDockedInfoMap.erase(dockedClientID);

	wstring& carrierName = nameToDockedInfoMap[dockedShipName].carrierName;

	uint carrierId = HkGetClientIdFromCharname(carrierName);
	if (carrierId != -1)
	{
		mobiledockClients[carrierId].iDockingModulesAvailable++;
	}
	//clear the carrier list info
	auto& carrierDockList = nameToCarrierInfoMap[carrierName].dockedShipList;
	for (auto& dockIter = carrierDockList.begin() ; dockIter != carrierDockList.end() ; )
	{
		if (*dockIter == dockedShipName)
		{
			carrierDockList.erase(dockIter);
			break;
		}
	}

	//finally, clear the docked ship info
	nameToDockedInfoMap.erase(dockedShipName);

	return Success;
}

void DockShipOnCarrier(uint dockingID, uint carrierID)
{

	// The client is free to dock, erase from the pending list and handle
	mapPendingDockingRequests.erase(dockingID);

	wstring dockingName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(dockingID));
	bool shipAlreadyDocked = false;

	wstring carrierName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(carrierID));
	if (idToDockedInfoMap.count(dockingID) && idToDockedInfoMap.at(dockingID)->carrierName == carrierName)
	{
		shipAlreadyDocked = true;
	}

	if (!shipAlreadyDocked && mobiledockClients[carrierID].iDockingModulesAvailable == 0)
	{
		PrintUserCmdText(dockingID, L"Carrier has no free docking capacity");
		pub::Player::SendNNMessage(dockingID, pub::GetNicknameId("info_access_denied"));
		return;
	}
	// Save the carrier info
	if (!shipAlreadyDocked)
	{
		//In case this ship was launched from another mobiledock, unregister it first.
		RemoveShipFromLists(dockingName, false);
		std::lock_guard<std::mutex> saveLock(saveMutex);

		nameToCarrierInfoMap[carrierName].dockedShipList.emplace_back(dockingName);
		nameToDockedInfoMap[dockingName].lastDockedSolar = Players[dockingID].iLastBaseID;
		nameToDockedInfoMap[dockingName].carrierName = carrierName;
		idToCarrierInfoMap[carrierID] = &nameToCarrierInfoMap[carrierName];
		idToDockedInfoMap[dockingID] = &nameToDockedInfoMap[dockingName];
		idToCarrierInfoMap[carrierID]->lastCarrierLogin = time(nullptr);
		mobiledockClients[carrierID].iDockingModulesAvailable--;
	}

	// Land the ship on the designated base
	HkBeamById(dockingID, mobileDockingProxyBase);
	PrintUserCmdText(carrierID, L"%ls successfully docked", dockingName.c_str());
}

void HkTimerCheckKick()
{
	returncode = DEFAULT_RETURNCODE;
	// DELAYEDDOCK vector iteration
	for (auto& dockdata = dockingInProgress.begin(); dockdata != dockingInProgress.end();)
	{
		auto& dd = dockdata->second;
		const auto& dockingShipID = Players[dd.dockingID].iShipID;
		if (dockingShipID == 0)
		{
			dockdata = dockingInProgress.erase(dockdata);
			continue;
		}
		Vector V1mov, V1rot;
		pub::SpaceObj::GetMotion(dockingShipID, V1mov, V1rot);
		if (V1mov.x > 5 || V1mov.y > 5 || V1mov.z > 5)
		{
			auto dockingName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(dd.dockingID));
			PrintUserCmdText(dd.dockingID, L"Docking aborted due to your movement");
			PrintUserCmdText(dd.carrierID, L"Docking aborted due to %ls's movement", dockingName);
			dockdata = dockingInProgress.erase(dockdata);
			continue;
		}

		Vector V2mov, V2rot;
		const auto& carrierShipID = Players[dd.carrierID].iShipID;
		pub::SpaceObj::GetMotion(carrierShipID, V2mov, V2rot);
		if (V2mov.x > 5 || V2mov.y > 5 || V2mov.z > 5)
		{
			PrintUserCmdText(dd.dockingID, L"Docking aborted due to carrier movement");
			PrintUserCmdText(dd.carrierID, L"Docking aborted due to carrier movement");
			dockdata = dockingInProgress.erase(dockdata);
			continue;
		}

		dd.timeLeft--;
		if (!dd.timeLeft)
		{
			Vector pos;
			Matrix dummy;
			pub::SpaceObj::GetLocation(dockingShipID, pos, dummy);
			float distance = HkDistance3D(pos, dd.startPosition);
			if (distance > maxDockingDistanceTolerance)
			{
				auto dockingName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(dd.dockingID));
				PrintUserCmdText(dd.dockingID, L"Docking aborted due to your movement");
				PrintUserCmdText(dd.carrierID, L"Docking aborted due to %ls's movement", dockingName);
				dockdata = dockingInProgress.erase(dockdata);
				continue;
			}
			DockShipOnCarrier(dd.dockingID, dd.carrierID);
			dockdata = dockingInProgress.erase(dockdata);
			continue;
		}

		PrintUserCmdText(dd.dockingID, L"Docking in %us", dd.timeLeft);
		dockdata++;
	}
}

void MoveOfflineShipToLastDockedSolar(const wstring& charName)
{
	//player offline, edit their character file to put them on last docked solar
	CAccount* acc = HkGetAccountByCharname(charName);
	if (!acc)
	{
		return;
	}
	wstring dirName;
	wstring charFilename;
	HkGetAccountDirName(charName, dirName);
	HkGetCharFileName(charName, charFilename);
	string charpath = scAcctPath + wstos(dirName) + "\\" + wstos(charFilename) + ".fl";
	const auto& dockedInfo = nameToDockedInfoMap[charName];
	const auto& baseInfo = Universe::get_base(dockedInfo.lastDockedSolar);
	const auto& sysInfo = Universe::get_system(baseInfo->iSystemID);
	if (baseInfo && sysInfo)
	{
		WritePrivateProfileString("Player", "system", sysInfo->nickname, charpath.c_str());
		WritePrivateProfileString("Player", "base", baseInfo->cNickname, charpath.c_str());
	}
	else
	{
		WritePrivateProfileString("Player", "system", defaultReturnSystem.c_str(), charpath.c_str());
		WritePrivateProfileString("Player", "base", defaultReturnBase.c_str(), charpath.c_str());
	}
	
	//remove the docked ship entry from carrier's data
	if (nameToCarrierInfoMap.count(dockedInfo.carrierName))
	{
		auto& dockedList = nameToCarrierInfoMap[dockedInfo.carrierName].dockedShipList;
		for (auto& iter = dockedList.begin(); iter != dockedList.end(); iter++)
		{
			if (*iter == charName)
			{
				std::lock_guard<std::mutex> saveLock(saveMutex);
				dockedList.erase(iter);

				uint carrierID = HkGetClientIdFromCharname(dockedInfo.carrierName);
				if (carrierID != -1)
				{
					mobiledockClients[carrierID].iDockingModulesAvailable++;
				}
				break;
			}
		}
	}
}

// Returns count of installed docking modules on ship of specific client.
uint GetInstalledModules(uint iClientID)
{
	uint modules = 0;

	// Check to see if the vessel undocking currently has a docking module equipped
	for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
	{
		if (item->bMounted && dockingModuleEquipmentCapacityMap.count(item->iArchID))
		{
			modules += dockingModuleEquipmentCapacityMap.at(item->iArchID);
		}
	}

	return modules;
}

void StartDockingProcedure(uint dockingID, uint carrierID)
{

	if (dockingPeriod)
	{
		uint shipId = Players[dockingID].iShipID;
		if (!shipId)
		{
			PrintUserCmdText(carrierID, L"ERR Docking procedure impossible, target ship is docked");
			PrintUserCmdText(dockingID, L"ERR Carrier docking procedure aborted, you're already docked");
			return;
		}
		if (disableShieldsOnDockAttempt)
		{
			pub::SpaceObj::DrainShields(shipId);
		}

		Vector pos;
		Matrix dummy;
		pub::SpaceObj::GetLocation(shipId, pos, dummy);

		DELAYEDDOCK dd;
		dd.carrierID = carrierID;
		dd.dockingID = dockingID;
		dd.timeLeft = dockingPeriod;
		dd.startPosition = pos;
		dockingInProgress[dockingID] = dd;
		wstring dockingName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(dockingID));
		wstring carrierName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(carrierID));
		wstring message = dockingName + L" has begun docking on " + carrierName;
		PrintLocalUserCmdText(dockingID, message, 10000);
	}
	else
	{
		DockShipOnCarrier(dockingID, carrierID);
	}
}

void AddClientToDockQueue(uint dockingID, uint carrierID)
{
	const auto& dockMode = mobiledockClients[carrierID].dockMode;
	if (dockMode == ALLOW_ALL)
	{
		StartDockingProcedure(dockingID, carrierID);
	}
	else if (dockMode == ALLOW_NONE)
	{
		mapPendingDockingRequests[dockingID] = carrierID;
		PrintUserCmdText(carrierID, L"%s is requesting to dock, authorise with /allowdock", Players.GetActiveCharacterName(dockingID));
		PrintUserCmdText(dockingID, L"Docking request sent to %s", Players.GetActiveCharacterName(carrierID));
	}
	else if (dockMode == ALLOW_GROUP)
	{
		list<GROUP_MEMBER> lstGrpMembers;
		HkGetGroupMembers((const wchar_t*)Players.GetActiveCharacterName(dockingID), lstGrpMembers);

		bool isGroupMember = false;
		for (auto& member : lstGrpMembers)
		{
			if (member.iClientID == carrierID)
			{
				isGroupMember = true;
				break;
			}
		}
		if (isGroupMember)
		{
			StartDockingProcedure(dockingID, carrierID);
		}
		else
		{
			mapPendingDockingRequests[dockingID] = carrierID;
			PrintUserCmdText(carrierID, L"%s is requesting to dock, authorise with /allowdock", Players.GetActiveCharacterName(dockingID));
			PrintUserCmdText(dockingID, L"Docking request sent to %s", Players.GetActiveCharacterName(carrierID));
		}
	}
}

void BeamToCarrier(uint dockedID, uint carrierShipID)
{
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

	jumpData.iClientID = dockedID;
	jumpData.iSystemID = iSystemID;
	jumpData.pos = pos;
	jumpData.ori = ori;

	Plugin_Communication(PLUGIN_MESSAGE::CUSTOM_JUMP_CALLOUT, &jumpData);
}

bool CanDockOnCarrier(uint dockingID, uint carrierShipID)
{
	if (mobiledockClients[carrierShipID].iDockingModulesAvailable > 0)
	{
		return true;
	}
	if (!idToCarrierInfoMap.count(carrierShipID))
	{
		return false;
	}
	wstring dockingName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(dockingID));
	for (wstring& dockedName : idToCarrierInfoMap.at(carrierShipID)->dockedShipList)
	{
		if (dockedName == dockingName)
		{
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall BaseExit(uint iBaseID, uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	mobiledockClients[iClientID].iDockingModulesInstalled = GetInstalledModules(iClientID);
	
	if (idToCarrierInfoMap.count(iClientID))
	{
		// Normalize the docking modules available, with the number of people currently docked
		mobiledockClients[iClientID].iDockingModulesAvailable = (mobiledockClients[iClientID].iDockingModulesInstalled - idToCarrierInfoMap[iClientID]->dockedShipList.size());
	}
	else
	{
		mobiledockClients[iClientID].iDockingModulesAvailable = mobiledockClients[iClientID].iDockingModulesInstalled;
	}

}

void __stdcall PlayerLaunch_AFTER(unsigned int ship, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;

	// If not docked on another ship, skip processing.
	if (Players[client].iLastBaseID != mobileDockingProxyBase || 
		(!idToDockedInfoMap.count(client) && !jettisonedShipsQueue.count(client)))
	{
		return;
	}

	uint carrierClientID = -1;
	if (jettisonedShipsQueue.count(client))
	{
		carrierClientID = jettisonedShipsQueue[client];
		jettisonedShipsQueue.erase(client);
	}
	else
	{
		carrierClientID = HkGetClientIdFromCharname(idToDockedInfoMap[client]->carrierName.c_str());
	}

	// If carrier is present at server - do it, if not - whatever. Plugin erases all associated client data after disconnect. 
	if (carrierClientID != -1)
	{
		uint carrierShipID;
		pub::Player::GetShip(carrierClientID, carrierShipID);
		if (carrierShipID == 0)
		{
			//carrier docked somewhere, undock into that base. POBs require special handling.
			CUSTOM_BASE_IS_DOCKED_STRUCT pobCheck;
			pobCheck.iClientID = carrierClientID;
			pobCheck.iDockedBaseID = 0;
			Plugin_Communication(PLUGIN_MESSAGE::CUSTOM_BASE_IS_DOCKED, &pobCheck);
			if (pobCheck.iDockedBaseID)
			{
				CUSTOM_BASE_BEAM_STRUCT pobBeam;
				pobBeam.iClientID = client;
				pobBeam.iTargetBaseID = pobCheck.iDockedBaseID;
				Plugin_Communication(PLUGIN_MESSAGE::CUSTOM_BASE_BEAM, &pobBeam);
			}
			else
			{
				uint baseID;
				pub::Player::GetBase(carrierClientID, baseID);
				HkBeamById(client, baseID);
			}
		}
		else
		{
			//check if carrier is currently mid-transit, if so, force dock him back to proxy base and abort;
			CUSTOM_IN_WARP_CHECK_STRUCT warpCheck;
			warpCheck.clientId = carrierClientID;
			Plugin_Communication(PLUGIN_MESSAGE::CUSTOM_IN_WARP_CHECK, &warpCheck);
			if (warpCheck.inWarp)
			{
				HkBeamById(client, mobileDockingProxyBase);
				PrintUserCmdText(client, L"ERR Can't undock while carrier is in hyperspace transit.");
				returncode = SKIPPLUGINS_NOFUNCTIONCALL;
				return;
			}

			BeamToCarrier(client, carrierShipID);
		}
	}
	else
	{
		CUSTOM_BASE_IS_DOCKED_STRUCT lastDockedCheck;
		lastDockedCheck.iClientID = client;
		lastDockedCheck.iDockedBaseID = 0;
		Plugin_Communication(PLUGIN_MESSAGE::CUSTOM_BASE_IS_DOCKED, &lastDockedCheck);
		if (lastDockedCheck.iDockedBaseID)
		{
			CUSTOM_BASE_BEAM_STRUCT POBbeamStruct;
			POBbeamStruct.iClientID = client;
			POBbeamStruct.iTargetBaseID = lastDockedCheck.iDockedBaseID;
			Plugin_Communication(PLUGIN_MESSAGE::CUSTOM_BASE_BEAM, &POBbeamStruct);
		}
		else
		{
			uint lastDockedBaseID = idToDockedInfoMap[client]->lastDockedSolar;
			if (lastDockedBaseID)
			{
				HkBeamById(client, lastDockedBaseID);
			}
		}
	}
}

// If this is a docking request at a player ship then process it.
int __cdecl Dock_Call(unsigned int const &iShip, unsigned int const &iTargetID, int& dockPort, enum DOCK_HOST_RESPONSE& response)
{
	returncode = DEFAULT_RETURNCODE;

	//if not a player dock, skip
	uint client = HkGetClientIDByShip(iShip);
	if (client && response == DOCK && dockPort == -1)
	{
		// If target not a player in FREIGHTER class ship, ignore request
		returncode = SKIPPLUGINS;

		uint iType;
		pub::SpaceObj::GetType(iTargetID, iType);
		if (!(iType & (OBJ_FREIGHTER | OBJ_TRANSPORT | OBJ_GUNBOAT | OBJ_CRUISER | OBJ_CAPITAL)))
		{
			return 0;
		}
		
		uint systemId;
		pub::SpaceObj::GetSystem(iShip, systemId);
		if (bannedSystems.count(systemId))
		{
			PrintUserCmdText(client, L"ERR Cannot mobile dock in this system!");
			dockPort = -1;
			response = DOCK_DENIED;
			return 0;
		}

		// If target is not player ship or ship is too far away then ignore the request.
		if (HkDistance3DByShip(iShip, iTargetID) > mobileDockingRange)
		{
			PrintUserCmdText(client, L"Ship is out of range");
			dockPort = -1;
			response = DOCK_DENIED;
			return 0;
		}

		uint iTargetClientID = HkGetClientIDByShip(iTargetID);

		// Check that the target ship has an empty docking module.
		if (!iTargetClientID)
		{
			PrintUserCmdText(client, L"Not a player ship!");
			dockPort = -1;
			response = DOCK_DENIED;
			return 0;
		}

		CUSTOM_CLOAK_CHECK_STRUCT cloakCheck;
		cloakCheck.clientId = client;
		Plugin_Communication(CUSTOM_CLOAK_CHECK, &cloakCheck);
		if (cloakCheck.isCloaked)
		{
			PrintUserCmdText(client, L"Cannot dock while cloaked!");
			dockPort = -1;
			response = DOCK_DENIED;
			return 0;
		}

		// If ship is already registered as 'docked' on this carrier, proceed and ignore the capacity.
		if (idToDockedInfoMap.count(client))
		{
			wstring targetName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iTargetClientID));
			if (idToDockedInfoMap.at(client)->carrierName == targetName)
			{
				AddClientToDockQueue(client, iTargetClientID);
				returncode = SKIPPLUGINS_NOFUNCTIONCALL;
				return 0;
			}
		}
		
		if (!CanDockOnCarrier(client, iTargetClientID))
		{

			PrintUserCmdText(client, L"Target ship has no free docking capacity");
			dockPort = -1;
			response = DOCK_DENIED;
			return 0;
		}

		// Check that the requesting ship is of the appropriate size to dock.
		const auto& shipInfo = Archetype::GetShip(Players[client].iShipArchetype);
		if (shipInfo->fHoldSize > cargoCapacityLimit)
		{
			PrintUserCmdText(client, L"Target ship cannot dock a ship of your size.");
			dockPort = -1;
			response = DOCK_DENIED;
			return 0;
		}

		// Create a docking request and send a notification to the target ship.
		AddClientToDockQueue(client, iTargetClientID);
		// Purposefully freeze the ship in space by stopping the function call
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
	}
	return 0;
}

void __stdcall BaseEnter(uint iBaseID, uint client)
{
	returncode = DEFAULT_RETURNCODE;
	if (!idToDockedInfoMap.count(client))
	{
		return;
	}

	uint carrierID = HkGetClientIdFromCharname(idToDockedInfoMap[client]->carrierName);

	if (iBaseID == mobileDockingProxyBase)
	{
		// Set the base name
		wstring status = L"<RDL><PUSH/>";
		status += L"<TEXT>Currently docked on: " + XMLText(idToDockedInfoMap[client]->carrierName);
		if (carrierID != -1)
		{
			// if carrier online, add system info.
			const auto& sysInfo = Universe::get_system(Players[carrierID].iSystemID);
			wstring& systemName = HkGetWStringFromIDS(sysInfo->strid_name);
			status += L", " + systemName;
		}
		status += L"</TEXT>";
		status += L"<POP/></RDL>";
		SendSetBaseInfoText2(client, status);
	}
	else
	{
		auto charName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(client));
		if (carrierID != -1)
		{
			PrintUserCmdText(carrierID, L"%s has left your garrison, reclaiming hangar space.", charName);
		}

		RemoveShipFromLists(charName, false);
	}
}

bool UserCmd_Process(uint client, const wstring& wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	if (wscCmd.find(L"/listdocked") == 0)
	{
		if (!idToCarrierInfoMap.count(client) || idToCarrierInfoMap[client]->dockedShipList.empty())
		{
			PrintUserCmdText(client, L"No ships currently docked, free capacity: %u", mobiledockClients[client].iDockingModulesAvailable);
			return true;
		}

		uint counter = 0;
		PrintUserCmdText(client, L"Docked ships:");
		for (wstring& dockedShip : idToCarrierInfoMap[client]->dockedShipList)
		{
			counter++;
			uint dockedClientID = HkGetClientIdFromCharname(dockedShip.c_str());
			if (dockedClientID != -1)
			{
				const auto& shipInfo = Archetype::GetShip(Players[dockedClientID].iShipArchetype);
				wstring& shipName = HkGetWStringFromIDS(shipInfo->iIdsName);
				uint ship;
				pub::Player::GetShip(dockedClientID, ship);
				if (ship)
				{
					PrintUserCmdText(client, L"%u. %ls - %ls - LAUNCHED", counter, dockedShip.c_str(), shipName.c_str());
				}
				else
				{
					PrintUserCmdText(client, L"%u. %ls - %ls - ON STANDBY", counter, dockedShip.c_str(), shipName.c_str());
				}
			}
			else
			{
				PrintUserCmdText(client, L"%u. %ls - OFFLINE", counter, dockedShip.c_str());
			}
		}
		if (mobiledockClients[client].iDockingModulesAvailable)
		{
			PrintUserCmdText(client, L"Remaining free capacity: %u", mobiledockClients[client].iDockingModulesAvailable);
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
			return true;
		}
		// Get the supposed ship we should be ejecting from the command parameters
		wstring& selectedShip = Trim(GetParam(wscCmd, ' ', 1));
		if (selectedShip.empty())
		{
			PrintUserCmdText(client, L"Usage: /jettisonship <charName/charNr>");
			return true;
		}
		uint charNumber = ToUInt(selectedShip);
		const auto& dockedShipList = idToCarrierInfoMap[client]->dockedShipList;

		if (charNumber != 0 && charNumber <= dockedShipList.size())
		{
			selectedShip = dockedShipList.at(charNumber - 1);
		}
		JettisonResult jettisonResult = RemoveShipFromLists(selectedShip, true);
		switch (jettisonResult)
		{
			case Success:
			{
				PrintUserCmdText(client, L"%ls jettisoned", selectedShip.c_str());
				break;
			}
			case InvalidName:
			{
				PrintUserCmdText(client, L"ERR this ship is not onboard!");
				break;
			}
			case ShipInSpace:
			{
				PrintUserCmdText(client, L"ERR this ship is already in space!");
			}
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
		if (!CanDockOnCarrier(iTargetClientID, client))
		{
			mapPendingDockingRequests.erase(iTargetClientID);
			PrintUserCmdText(client, L"No free docking modules available.");
			return true;
		}

		StartDockingProcedure(iTargetClientID, client);
		return true;
	}
	else if (wscCmd.find(L"/dockmode") == 0)
	{
		if (mobiledockClients[client].iDockingModulesInstalled == 0)
		{
			PrintUserCmdText(client, L"ERR No docking modules installed");
			return true;
		}
		wstring& param = GetParam(wscCmd, ' ', 1);
		if (param.empty())
		{
			PrintUserCmdText(client, L"Usage: /dockmode <all/group/none>");
		}
		else if (param == L"all")
		{
			mobiledockClients[client].dockMode = ALLOW_ALL;
			PrintUserCmdText(client, L"Dockmode set to ALL");
		}
		else if (param == L"group")
		{
			mobiledockClients[client].dockMode = ALLOW_GROUP;
			PrintUserCmdText(client, L"Dockmode set to GROUP");
		}
		else if (param == L"none")
		{
			mobiledockClients[client].dockMode = ALLOW_NONE;
			PrintUserCmdText(client, L"Dockmode set to NONE");
		}
		else
		{
			PrintUserCmdText(client, L"Usage: /dockmode <all/group/none>");
		}
	}
	return false;
}

void __stdcall DisConnect(unsigned int iClientID, enum  EFLConnection state)
{
	if (idToCarrierInfoMap.count(iClientID))
	{
		for (const wstring& dockedPlayer : idToCarrierInfoMap[iClientID]->dockedShipList)
		{
			uint dockedClientID = HkGetClientIdFromCharname(dockedPlayer.c_str());
			if (dockedClientID != -1)
			{
				wstring& lastBaseName = GetLastBaseName(dockedClientID);
				PrintUserCmdText(dockedClientID, L"Carrier logged off, redirecting undock to %ls", lastBaseName);
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
	
	wstring charname = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID));
	if (nameToCarrierInfoMap.count(charname))
	{
		idToCarrierInfoMap[iClientID] = &nameToCarrierInfoMap[charname];
		for (const wstring& dockedShipName : idToCarrierInfoMap[iClientID]->dockedShipList)
		{
			uint dockedClientID = HkGetClientIdFromCharname(dockedShipName.c_str());
			if (dockedClientID != -1)
			{
				PrintUserCmdText(dockedClientID, L"Carrier ship has come online.");
			}
		}
		std::lock_guard<std::mutex> saveLock(saveMutex);
		idToCarrierInfoMap[iClientID]->lastCarrierLogin = time(nullptr);
	}
	else if (nameToDockedInfoMap.count(charname))
	{
		idToDockedInfoMap[iClientID] = &nameToDockedInfoMap[charname];
		uint carrierClientID = HkGetClientIdFromCharname(idToDockedInfoMap[iClientID]->carrierName.c_str());
		if (carrierClientID != -1)
		{
			const auto& shipInfo = Archetype::GetShip(Players[iClientID].iShipArchetype);
			wstring& shipName = HkGetWStringFromIDS(shipInfo->iIdsName);
			PrintUserCmdText(carrierClientID, L"INFO: %ls, %ls is standing by for launch", shipName.c_str(), charname);

			const auto& carrierInfo = Players[carrierClientID];
			const auto& sysInfo = Universe::get_system(carrierInfo.iSystemID);
			wstring& sysName = HkGetWStringFromIDS(sysInfo->strid_name);
			if (carrierInfo.iShipID)
			{
				Vector pos;
				Matrix ori;
				pub::SpaceObj::GetLocation(carrierInfo.iShipID, pos, ori);
				wstring& sector = VectorToSectorCoord(sysInfo->id, pos);
				PrintUserCmdText(iClientID, L"Carrier %ls in flight, %ls %ls", idToDockedInfoMap[iClientID]->carrierName.c_str(), sysName.c_str(), sector.c_str());
			}
			else if (carrierInfo.iBaseID)
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
			wstring& lastBaseName = GetLastBaseName(iClientID);

			PrintUserCmdText(iClientID, L"Carrier currently offline. Last docked base: %ls", lastBaseName);
		}
	}
}

void __stdcall UseItemRequest(SSPUseItem const& p1, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;
	if (disableRegensOnDockAttempt && dockingInProgress.count(iClientID))
	{
		static uint rejectSound = CreateID("ui_select_reject");
		pub::Audio::PlaySoundEffect(iClientID, rejectSound);
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
	}
}

void Plugin_Communication_CallBack(PLUGIN_MESSAGE msg, void* data)
{
	returncode = DEFAULT_RETURNCODE;

	if (msg == CUSTOM_MOBILE_DOCK_CHECK)
	{
		CUSTOM_MOBILE_DOCK_CHECK_STRUCT* mobileDockCheck = reinterpret_cast<CUSTOM_MOBILE_DOCK_CHECK_STRUCT*>(data);
		if (idToDockedInfoMap.count(mobileDockCheck->iClientID))
		{
			mobileDockCheck->isMobileDocked = true;
		}
	}
	else if (msg == CUSTOM_RENAME_NOTIFICATION)
	{
		CUSTOM_RENAME_NOTIFICATION_STRUCT* info = reinterpret_cast<CUSTOM_RENAME_NOTIFICATION_STRUCT*>(data);
		if (nameToDockedInfoMap.count(info->currentName))
		{
			MoveOfflineShipToLastDockedSolar(info->currentName);
		}
	}
	else if (msg == CUSTOM_RESTART_NOTIFICATION)
	{
		CUSTOM_RESTART_NOTIFICATION_STRUCT* info = reinterpret_cast<CUSTOM_RESTART_NOTIFICATION_STRUCT*>(data);
		if (nameToDockedInfoMap.count(info->playerName))
		{
			MoveOfflineShipToLastDockedSolar(info->playerName);
		}
	}
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
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
	p_PI->bMayPause = false;
	p_PI->bMayUnload = false;
	p_PI->ePluginReturnCode = &returncode;
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch_AFTER, PLUGIN_HkIServerImpl_PlayerLaunch_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterSelect_AFTER, PLUGIN_HkIServerImpl_CharacterSelect_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&DisConnect, PLUGIN_HkIServerImpl_DisConnect, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkTimerCheckKick, PLUGIN_HkTimerCheckKick, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UseItemRequest, PLUGIN_HkIServerImpl_SPRequestUseItem, 0));

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 3));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Dock_Call, PLUGIN_HkCb_Dock_Call, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseEnter, PLUGIN_HkIServerImpl_BaseEnter, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseExit, PLUGIN_HkIServerImpl_BaseExit, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Plugin_Communication_CallBack, PLUGIN_Plugin_Communication, 12));

	saveThread = std::thread(SaveData);

	return p_PI;
}
