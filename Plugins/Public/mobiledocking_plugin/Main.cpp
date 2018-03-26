#include "Main.h"

PLUGIN_RETURNCODE returncode;
map<uint, CLIENT_DATA> mobiledockClients;
map<uint, uint> mapPendingDockingRequests;
vector<uint> dockingModuleEquipmentIds;

uint connSystemID = CreateID("li06");

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

	// The path to the data file.
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	// Create the directory if it doesn't exist
	string moddir = string(datapath) + R"(\Accts\MultiPlayer\docking_module\)";
	CreateDirectoryA(moddir.c_str(), 0);

	//@@TODO Add support for defining multiple docking modules in the configuration file
	// Add the current available docking module to the list of available arches
	dockingModuleEquipmentIds.push_back(CreateID("dsy_docking_module_1"));
}

void __stdcall BaseExit(uint iBaseID, uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	// Check to see if the vessel undocking currently has a docking module equipped
	for(list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
	{
		if(find(dockingModuleEquipmentIds.begin(), dockingModuleEquipmentIds.end(), item->iArchID) != dockingModuleEquipmentIds.end())
		{
			if(item->bMounted)
			{
				mobiledockClients[iClientID].iDockingModulesInstalled++;
			}
		}
	}

	// Normalize the docking modules available, with the number of people currently docked
	mobiledockClients[iClientID].iDockingModulesAvailable = (mobiledockClients[iClientID].iDockingModulesInstalled - mobiledockClients[iClientID].mapDockedShips.size());

	// If this is a ship which is currently docked, clear the market
	if(mobiledockClients[iClientID].mobileDocked)
	{
		SendResetMarketOverride(iClientID);
	}

}

void __stdcall PlayerLaunch(unsigned int iShip, unsigned int client)
{
	returncode = SKIPPLUGINS;

	uint carrier_client = HkGetClientIdFromCharname(mobiledockClients[client].wscDockedWithCharname);

	wstring clientName = (const wchar_t*)Players.GetActiveCharacterName(client);

	if (mobiledockClients[client].mobileDocked)
	{
		// Update the location of the carrier and remove the docked ship from the carrier
		if (carrier_client != -1)
		{
			mobiledockClients[carrier_client].mapDockedShips.erase(clientName);
			mobiledockClients[carrier_client].iDockingModulesAvailable++;
		}
		else
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
		if(!carrierShip)
		{

			uint iBaseID;
			pub::Player::GetBase(carrier_client, iBaseID);

			if(!iBaseID)
			{
				return;
			}

			// Set the flags which the PlayerLaunch_AFTER uses to handle teleporting to the carriers base
			mobiledockClients[client].undockBase = Universe::get_base(iBaseID);
			mobiledockClients[client].baseUndock = true;

			return;
		}

		// Update the internal values of the docked ship pretaining to the carrier
		UpdateCarrierLocationInformation(client, carrierShip);

		// Now that the data is prepared, send the player to the carriers location
		JumpToLocation(client, mobiledockClients[client].carrierSystem, mobiledockClients[client].carrierPos, mobiledockClients[client].carrierRot);
		
		// Clear the client out of the mobiledockClients now that it's no longer docked
		mobiledockClients.erase(client);

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
					wstring wscMsgU = L"MF: %name has been banned. (Type 6)";
					wscMsgU = ReplaceStr(wscMsgU, L"%name", playerName.c_str());

					HkMsgU(wscMsgU);

					wstring wscMsgLog = L"<%sender> was banned for undocking from a carrier in conn with cargo.";
					wscMsgLog = ReplaceStr(wscMsgLog, L"%sender", playerName.c_str());

					LogCheater(client, wscMsgLog);
				}
			}
		}



	}
}

void __stdcall PlayerLaunch_AFTER(unsigned int ship, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;

	// Is the client flagged to dock at a base after exiting?
	if(mobiledockClients[client].baseUndock)
	{
		pub::Player::ForceLand(client, mobiledockClients[client].undockBase->iBaseID);
		mobiledockClients.erase(client);
	}

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

		// Check that the target ship has an empty docking module.
		if (mobiledockClients[iTargetClientID].iDockingModulesAvailable == 0)
		{
			PrintUserCmdText(client, L"Target ship has no free docking capacity");
			return 0;
		}

		// Check that the requesting ship is of the appropriate size to dock.
		CShip* cship = dynamic_cast<CShip*>(HkGetEqObjFromObjRW(reinterpret_cast<IObjRW*>(HkGetInspect(client))));
		if (cship->shiparch()->fHoldSize > 275)
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

	if (mobiledockClients[client].mobileDocked)
	{
		// Clear the market. We don't support transfers in this version.
		SendResetMarketOverride(client);

		// Set the base name
		wstring status = L"<RDL><PUSH/>";
		status += L"<TEXT>" + XMLText(mobiledockClients[client].wscDockedWithCharname) + L"</TEXT><PARA/><PARA/>";
		status += L"<POP/></RDL>";
		SendSetBaseInfoText2(client, status);

		// Check to see that the carrier thinks this ship is docked to it.
		// If it isn't then eject the ship to space.
		wstring charname = (const wchar_t*)Players.GetActiveCharacterName(client);
		if (!IsShipDockedOnCarrier(mobiledockClients[client].wscDockedWithCharname, charname))
		{
			// Update the carrier location
			uint carrierShip;
			pub::Player::GetShip(client, carrierShip);
			UpdateCarrierLocationInformation(client, carrierShip);

			JumpToLocation(client,
				mobiledockClients[client].carrierSystem,
				mobiledockClients[client].carrierPos,
				mobiledockClients[client].carrierRot);
			return;
		}
	}

}

void __stdcall ShipDestroyed(DamageList *_dmg, DWORD *ecx, uint kill)
{
	returncode = DEFAULT_RETURNCODE;

	CShip *cship = (CShip*)ecx[4];
	uint client = cship->GetOwnerPlayer();
	Vector shipPosition = cship->get_position();
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

					// Make sure that each docked ship agrees that it's docked before sending a teleport order
					if(!mobiledockClients[iDockedClientID].mobileDocked)
					{
						mobiledockClients[client].mapDockedShips.erase(i->first);
						continue;
					}

					// Update the coordinates the given ship should launch to.
					UpdateCarrierLocationInformation(iDockedClientID, cship->get_position(), cship->get_orientation());

					// Due to the carrier not existing anymore, we have to pull the system information from the carriers historical location.
					mobiledockClients[iDockedClientID].carrierSystem = mobiledockClients[client].carrierSystem;

					if (iDockedClientID)
					{
						JumpToLocation(iDockedClientID,
							mobiledockClients[iDockedClientID].carrierSystem,
							mobiledockClients[iDockedClientID].carrierPos,
							mobiledockClients[iDockedClientID].carrierRot);
					}
				}

				// Clear the carrier from the list
				mobiledockClients[client].mapDockedShips.clear();
			}
			// If this was last docked at a carrier then set the last base to the to
			// last real base the ship docked at.
			else if (mobiledockClients[client].iLastBaseID)
			{
				Players[client].iLastBaseID = mobiledockClients[client].iLastBaseID;
				mobiledockClients[client].iLastBaseID = 0;
			}
		}
	}
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
	else if(wscCmd.find(L"/conn") == 0)
	{
		// This plugin always runs before playercntl runs it's conn function. Verify that there are no docked ships.
		if(!mobiledockClients[client].mapDockedShips.empty())
		{
			PrintUserCmdText(client, L"You cannot use this command if you have vessels docked with you!");
			returncode = SKIPPLUGINS;
		}
	}
	else if(wscCmd.find(L"/jettisonship") == 0)
	{
		// Get the supposed ship we should be ejecting from the command parameters
		wstring charname = Trim(GetParam(wscCmd, ' ', 1));
		if(charname.empty())
		{
			PrintUserCmdText(client, L"Usage: /jettisonship <charname>");
			return true;
		}

		// Only allow jettisonning a ship if the carrier is undocked
		uint carrierShip;
		pub::Player::GetShip(client, carrierShip);
		if(!carrierShip)
		{
			PrintUserCmdText(client, L"You can only jettison a vessel if you are in space.");
			return true;
		}


		// Check to see if the user listed is actually docked with the carrier at the moment
		if(mobiledockClients[client].mapDockedShips.find(charname) == mobiledockClients[client].mapDockedShips.end())
		{
			PrintUserCmdText(client, L"%s is not docked with you!", charname);
			return true;
		}

		// The player exists. Remove him from the docked list, and kick him into space
		const uint iDockedClientID = HkGetClientIdFromCharname(charname);
		if(iDockedClientID)
		{
			uint carrierShip;
			pub::Player::GetShip(client, carrierShip);

			// Update the client with the current carrier location
			UpdateCarrierLocationInformation(iDockedClientID, carrierShip);

			//Force the docked ship to launch. The teleport coordinates have been set by the previous method
			const CLIENT_DATA& dockedShipData = mobiledockClients[iDockedClientID];
			JumpToLocation(iDockedClientID, dockedShipData.carrierSystem, dockedShipData.carrierPos, dockedShipData.carrierRot);
		}

		mobiledockClients[client].iDockingModulesAvailable++;
		mobiledockClients.erase(iDockedClientID);
		mobiledockClients[client].mapDockedShips.erase(charname);
		PrintUserCmdText(client, L"Ship jettisoned");
		return true;

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
		if (mobiledockClients[client].iDockingModulesAvailable <= 0)
		{
			mapPendingDockingRequests.erase(iTargetClientID);
			PrintUserCmdText(client, L"No free docking modules available.");
			return true;
		}

		// Check if we're in conn. If so, reject the request
		uint clientSystem;
		pub::SpaceObj::GetSystem(client, clientSystem);
		if(clientSystem == connSystemID)
		{
			mapPendingDockingRequests.erase(iTargetClientID);
			PrintUserCmdText(client, L"You cannot dock in Connecticut.");
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
		pub::SpaceObj::GetSystem(iShip, mobiledockClients[client].carrierSystem);
		if (mobiledockClients[client].iLastBaseID != 0)
			mobiledockClients[client].iLastBaseID = Players[client].iLastBaseID;

		// Save the docking ship info
		mobiledockClients[iTargetClientID].mobileDocked = true;
		mobiledockClients[iTargetClientID].wscDockedWithCharname = (const wchar_t*)Players.GetActiveCharacterName(client);
		if (mobiledockClients[iTargetClientID].iLastBaseID != 0)
			mobiledockClients[iTargetClientID].iLastBaseID = Players[iTargetClientID].iLastBaseID;
		pub::SpaceObj::GetSystem(iShip, mobiledockClients[iTargetClientID].carrierSystem);

		mobiledockClients[client].iDockingModulesAvailable--;

		// Land the ship on the proxy base
		pub::Player::ForceLand(iTargetClientID, iBaseID);
		PrintUserCmdText(client, L"Ship docked");

		return true;
	}
	return false;
}

void __stdcall JumpInComplete(uint iSystemID, uint iShip)
{
	returncode = DEFAULT_RETURNCODE;
	const uint iClientID = HkGetClientIDByShip(iShip);
	
	// After completing a jump, if it's in our map, we update the current system
	if(mobiledockClients.find(iClientID) != mobiledockClients.end())
	{
		pub::SpaceObj::GetSystem(iShip, mobiledockClients[iClientID].carrierSystem);
	}
	
}

void __stdcall CharacterSelect(struct CHARACTER_ID const & cId, unsigned int iClientID)
{

	returncode = DEFAULT_RETURNCODE;
	
	// Attempt to load the data for this ship if it exists. Trim off the .fl at the end of the file name.
	string charFilename = cId.szCharFilename;
	charFilename = charFilename.substr(0, charFilename.size() - 3);

	LoadShip(charFilename);

	//If the user is a carrier, update the number of available modules
	if(mobiledockClients[iClientID].iDockingModulesAvailable != 0)
	{
		// Check how many modules are currently installed on the player, and normalize the storage variables.
		for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
		{
			if (find(dockingModuleEquipmentIds.begin(), dockingModuleEquipmentIds.end(), item->iArchID) != dockingModuleEquipmentIds.end())
			{
				if (item->bMounted)
				{
					mobiledockClients[iClientID].iDockingModulesInstalled++;
				}
			}
		}

		// Normalize the docking modules available, with the number of people currently docked
		mobiledockClients[iClientID].iDockingModulesAvailable = (mobiledockClients[iClientID].iDockingModulesInstalled - mobiledockClients[iClientID].mapDockedShips.size());
	}

}

void __stdcall DisConnect(uint iClientID, enum EFLConnection p2)
{

	returncode = DEFAULT_RETURNCODE;
	// Is the disconnecting user a part of the docking module plugin at the moment?
	if(mobiledockClients.find(iClientID) != mobiledockClients.end())
	{
		// Is this a carrier?
		if(!mobiledockClients[iClientID].mapDockedShips.empty())
		{
			wstring shipFileName;
			wstring charName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID));

			HkGetCharFileName(charName, shipFileName);
			SaveDockInfoCarrier(shipFileName ,iClientID, mobiledockClients[iClientID]);
		}

		// Is this a carried ship?
		else if(!empty(mobiledockClients[iClientID].wscDockedWithCharname))
		{
			wstring shipFileName;
			wstring charName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID));

			HkGetCharFileName(charName, shipFileName);
			SaveDockInfoCarried(shipFileName, iClientID, mobiledockClients[iClientID]);
		}

		mobiledockClients.erase(iClientID);
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
		if (set_scCfgFile.length()>0)
			LoadSettings();
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
	}
	return true;
}

void SystemSwitchOutComplete(unsigned int iShip, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
	static PBYTE SwitchOut = 0;
	if (!SwitchOut)
	{
		SwitchOut = (PBYTE)hModServer + 0xf600;

		DWORD dummy;
		VirtualProtect(SwitchOut + 0xd7, 200, PAGE_EXECUTE_READWRITE, &dummy);
	}

	// Patch the system switch out routine to put the ship in a
	// system of our choosing.
	if (mapDeferredJumps.find(client) != mapDeferredJumps.end())
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		SwitchOut[0x0d7] = 0xeb;				// ignore exit object
		SwitchOut[0x0d8] = 0x40;
		SwitchOut[0x119] = 0xbb;				// set the destination system
		*(PDWORD)(SwitchOut + 0x11a) = mapDeferredJumps[client].system;
		SwitchOut[0x266] = 0x45;				// don't generate warning
		*(float*)(SwitchOut + 0x2b0) = mapDeferredJumps[client].pos.z;		// set entry location
		*(float*)(SwitchOut + 0x2b8) = mapDeferredJumps[client].pos.y;
		*(float*)(SwitchOut + 0x2c0) = mapDeferredJumps[client].pos.x;
		*(float*)(SwitchOut + 0x2c8) = mapDeferredJumps[client].rot.data[2][2];
		*(float*)(SwitchOut + 0x2d0) = mapDeferredJumps[client].rot.data[1][1];
		*(float*)(SwitchOut + 0x2d8) = mapDeferredJumps[client].rot.data[0][0];
		*(float*)(SwitchOut + 0x2e0) = mapDeferredJumps[client].rot.data[2][1];
		*(float*)(SwitchOut + 0x2e8) = mapDeferredJumps[client].rot.data[2][0];
		*(float*)(SwitchOut + 0x2f0) = mapDeferredJumps[client].rot.data[1][2];
		*(float*)(SwitchOut + 0x2f8) = mapDeferredJumps[client].rot.data[1][0];
		*(float*)(SwitchOut + 0x300) = mapDeferredJumps[client].rot.data[0][2];
		*(float*)(SwitchOut + 0x308) = mapDeferredJumps[client].rot.data[0][1];
		*(PDWORD)(SwitchOut + 0x388) = 0x03ebc031;		// ignore entry object
		mapDeferredJumps.erase(client);

		pub::SpaceObj::SetInvincible(iShip, false, false, 0);
		Server.SystemSwitchOutComplete(iShip, client);

		// Unpatch the code.
		SwitchOut[0x0d7] = 0x0f;
		SwitchOut[0x0d8] = 0x84;
		SwitchOut[0x119] = 0x87;
		*(PDWORD)(SwitchOut + 0x11a) = 0x1b8;
		*(PDWORD)(SwitchOut + 0x25d) = 0x1cf7f;
		SwitchOut[0x266] = 0x1a;
		*(float*)(SwitchOut + 0x2b0) =
			*(float*)(SwitchOut + 0x2b8) =
			*(float*)(SwitchOut + 0x2c0) = 0;
		*(float*)(SwitchOut + 0x2c8) =
			*(float*)(SwitchOut + 0x2d0) =
			*(float*)(SwitchOut + 0x2d8) = 1;
		*(float*)(SwitchOut + 0x2e0) =
			*(float*)(SwitchOut + 0x2e8) =
			*(float*)(SwitchOut + 0x2f0) =
			*(float*)(SwitchOut + 0x2f8) =
			*(float*)(SwitchOut + 0x300) =
			*(float*)(SwitchOut + 0x308) = 0;
		*(PDWORD)(SwitchOut + 0x388) = 0xcf8b178b;
	}
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
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SystemSwitchOutComplete, PLUGIN_HkIServerImpl_SystemSwitchOutComplete, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch_AFTER, PLUGIN_HkIServerImpl_PlayerLaunch_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ShipDestroyed, PLUGIN_ShipDestroyed, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&JumpInComplete, PLUGIN_HkIServerImpl_JumpInComplete_AFTER, 0));

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 3));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Dock_Call, PLUGIN_HkCb_Dock_Call, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseEnter, PLUGIN_HkIServerImpl_BaseEnter, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseExit, PLUGIN_HkIServerImpl_BaseExit, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch, PLUGIN_HkIServerImpl_PlayerLaunch, 0));

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&DisConnect, PLUGIN_HkIServerImpl_DisConnect, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterSelect, PLUGIN_HkIServerImpl_CharacterSelect, 0));

	return p_PI;
}