// Docking Modules by Invoker
// September 2019

#include "Main.h"

PLUGIN_RETURNCODE returncode;

// Lists of items, used in dock plugin
map<uint, AMMO> mapAmmo;
map<uint, uint> mapBatteries;
map<uint, uint> mapBoostedAmmo;

ID_TRAITS defaultTraits;

// IDs, must be extracted from config file, but there are always default values if something goes wrong.
uint ID_object_lootcrate = CreateID("lootcrate_ast_loot_metal");
uint ID_item_nanobots = CreateID("ge_s_repair_01");
uint ID_item_batteries = CreateID("ge_s_battery_01");
uint ID_sound_accepted = CreateID("sfx_ui_react_processing01");
uint ID_sound_canceled = CreateID("sfx_ui_react_accept01");
uint ID_sound_docked = CreateID("tractor_loot_grabbed");
uint ID_sound_undocked = CreateID("cargo_jettison");
uint ID_sound_resupply = CreateID("depot_open_sound");
uint ID_sound_jumped = CreateID("jump_in");

// Docking requests and already docking ships
uint mapPendingDockingRequests[MAX_CLIENT_ID + 1];
uint mapDockingClients[MAX_CLIENT_ID + 1];

// Values to be handled in Send_FLPACKET_SERVER_LAUNCH and PlayerLaunch_AFTER
uint ShipsToLaunch[MAX_CLIENT_ID + 1];
uint ForceLandingClients[MAX_CLIENT_ID + 1];
uint POBForceLandingClients[MAX_CLIENT_ID + 1];

bool JumpingCarriers[MAX_CLIENT_ID + 1];
bool JumpingDockedShips[MAX_CLIENT_ID + 1];
bool JettisoningClients[MAX_CLIENT_ID + 1];
bool ResupplyingClients[MAX_CLIENT_ID + 1];

vector<ActionDocking> dockingQueues[MAX_CLIENT_ID + 1];

map<uint, DEFERREDJUMPS> mapDeferredJumps;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Loading Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	// Plugin configuration
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\specdock.cfg";
	string scAmmolimitCfgFile = string(szCurDir) + "\\flhook_plugins\\ammolimits.cfg";
	string scAutobuyCfgFile = string(szCurDir) + "\\flhook_plugins\\autobuy.cfg";

	// Get data directory.
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);
	dataPath = string(datapath);

	// Give access to the block of memory so system switch routine can be patched properly.
	DWORD dummy;
	VirtualProtect(SwitchOut + 0xd7, 200, PAGE_EXECUTE_READWRITE, &dummy);

	INI_Reader ini;

	// Read dock config file
	if (ini.open(scPluginCfgFile.c_str(), false))
	{
		uint traitCount = 0;

		while (ini.read_header())
		{
			if (ini.is_header("Main"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("allowedmodule"))
					{
						MODULE_ARCH arch;
						arch.maxCargoCapacity = ini.get_value_int(0);
						arch.dockingTime = ini.get_value_int(1);
						arch.basicResupplyTime = ini.get_value_int(2);
						arch.minCrewLimit = ini.get_value_int(3);
						arch.dockDisatnce = ini.get_value_int(4);
						arch.undockDistance = ini.get_value_int(5);

						uint archID = CreateID(ini.get_value_string(6));
						Watcher.moduleArchInfo[archID] = arch;
					}
				}
			}
			else if (ini.is_header("Audio"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("ID_sound_accepted"))
					{
						ID_sound_accepted = CreateID(ini.get_value_string());
					}
					else if (ini.is_value("ID_sound_canceled"))
					{
						ID_sound_canceled = CreateID(ini.get_value_string());
					}
					else if (ini.is_value("ID_sound_docked"))
					{
						ID_sound_docked = CreateID(ini.get_value_string());
					}
					else if (ini.is_value("ID_sound_undocked"))
					{
						ID_sound_undocked = CreateID(ini.get_value_string());
					}
					else if (ini.is_value("ID_sound_resupply"))
					{
						ID_sound_resupply = CreateID(ini.get_value_string());
					}
					else if (ini.is_value("ID_sound_jumped"))
					{
						ID_sound_jumped = CreateID(ini.get_value_string());
					}
				}
			}
			else if (ini.is_header("Support"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("ID_item_nanobots"))
					{
						ID_item_nanobots = CreateID(ini.get_value_string());
					}
					else if (ini.is_value("ID_item_batteries"))
					{
						ID_item_batteries = CreateID(ini.get_value_string());
					}
					else if (ini.is_value("ID_object_lootcrate"))
					{
						ID_object_lootcrate = CreateID(ini.get_value_string());
					}
				}
			}
			else if (ini.is_header("Supplies"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("boost"))
					{
						uint ammo = CreateID(ini.get_value_string(0));
						uint multiplier = ini.get_value_int(1);
						mapBoostedAmmo[ammo] = multiplier;
					}
				}
			}
			else if (ini.is_header("Traits"))
			{
				traitCount++;

				ID_TRAITS traits;
				vector<uint> IDs;
				bool isDefault = false;

				while (ini.read_value())
				{
					if (ini.is_value("$INHERIT_FROM_DEFAULT"))
						traits = defaultTraits;
					else if (ini.is_value("IDs"))
					{
						if (stows(ini.get_value_string(0)) != L"$DEFAULT")
						{
							for (string &param : GetParams((string)ini.get_value_string(), ','))
								IDs.push_back(CreateID((param).c_str()));
						}
						else
						{
							isDefault = true;
						}
					}
					else if (ini.is_value("proxy_base_suffix"))
					{
						traits.proxyBaseSuffix = ini.get_value_string(0);
					}
					else if (ini.is_value("crew_limit_multiplier"))
					{
						traits.crewLimitMultiplier = ini.get_value_float(0);
					}
					else if (ini.is_value("crew_item"))
					{
						traits.suitableCrewIDs.push_back(CreateID(ini.get_value_string(0)));
					}
					else if (ini.is_value("supply_item"))
					{
						SUPPLY item;
						item.ammoPerUnit = ini.get_value_int(0);
						item.batsPerUnit = ini.get_value_int(1);
						item.botsPerUnit = ini.get_value_int(2);
						item.cloakBatsPerUnit = ini.get_value_int(3);
						item.hullPerUnit = ini.get_value_int(4);

						uint itemID = CreateID(ini.get_value_string(5));
						traits.supplyItems[itemID] = item;
					}
				}

				if (isDefault)
					defaultTraits = traits;
				else
					for (uint &id : IDs)
						Watcher.IDTraits[id] = traits;
			}
		}

		ini.close();

		for (byte i = 0; i != MAX_CLIENT_ID + 2; ++i)
			dockingQueues[i].reserve(5);

		// Check immediately if players have installed docking modules if plugin was loaded from console.
		for (HKPLAYERINFO &player : HkGetPlayers())
			ModuleWatcher::CharacterSelect_AFTER(CHARACTER_ID(), player.iClientID);

		ConPrint(L"DOCK: Loaded %u docking modules and %u ID configurations.\n", Watcher.moduleArchInfo.size(), traitCount);
	}
	else
	{
		ConPrint(L"DOCK: Failed to read from dockingmodules.cfg. Plugin disabled.\n");
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

		ConPrint(L"DOCK: Loaded %u ammolimits. Efficiency boosted for %u ammo.\n", mapAmmo.size(), mapBoostedAmmo.size());
	}
	else
	{
		ConPrint(L"DOCK: Failed to read from ammolimits.cfg. Ammo resupplying disabled.\n");
	}

	// Read autobuy config file
	if (ini.open(scAutobuyCfgFile.c_str(), false))
	{
		uint stackables = 0;

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
						{
							mapAmmo[gunID].stackable = true;
							stackables++;
						}
					}
				}
			}
		}

		ini.close();

		ConPrint(L"DOCK: Loaded %u extra item pairs and %u stackable ammo.\n", mapBatteries.size(), stackables);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Hooked functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

int __cdecl Dock_Call(uint const &iShip, uint const &iBaseID, int iCancel, enum DOCK_HOST_RESPONSE response)
{
	returncode = DEFAULT_RETURNCODE;

	uint iClientID = HkGetClientIDByShip(iShip);

	// If target is not player ship - ignore request.
	// In case if player tries to dock to ship, iBaseID is actually iShipID.
	const uint iTargetClientID = HkGetClientIDByShip(iBaseID);
	if (!iTargetClientID)
		return 0;

	vector<MODULE_CACHE> &Modules = Watcher.Cache[iTargetClientID].Modules;
	wstring charname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
	ErrorMessage msg = TryDockInSpace(Modules, iClientID, iTargetClientID, iShip, iBaseID);

	switch (msg)
	{
	case NO_MODULES:		PrintUserCmdText(iClientID, L"The ship has no docking modules."); break;
	case TOO_LARGE:			PrintUserCmdText(iClientID, L"Your ship is too large to dock."); break;
	case NO_FREE_MODULES:	PrintUserCmdText(iClientID, L"The carrier has no free docking modules for you."); break;
	case TOO_FAR:			PrintUserCmdText(iClientID, L"Your ship is too far from carrier."); break;
	case OK:				returncode = SKIPPLUGINS_NOFUNCTIONCALL; return -1;
	}

	return 0;
}

void __stdcall RequestCancel(int iType, uint iShip, uint iBaseID, ulong p4, uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	// If the ship requested docking earlier - cancel request.
	CancelRequest(iClientID);
}

void __stdcall ReqShipArch_AFTER(uint iArchID, uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	// If ship requested docking earlier - cancel request.
	// Because ship cargo hold size may change after this and docking request becomes irrelevant.
	mapPendingDockingRequests[iClientID] = 0;
}

void __stdcall BaseEnter_AFTER(uint iBaseID, uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	if (Clients[iClientID].DockedToModule)
	{
		// Clear the market. We don't support transfers in this version.
		SendResetMarketOverride(iClientID);

		wstring carrierCharname = Clients[iClientID].DockedWith;
		uint carrierClientID = HkGetClientIdFromCharname(carrierCharname);

		// Set the base description.
		wstring status = L"<RDL><PUSH/>";
		status += L"<TEXT>Docked to " + XMLText(carrierCharname) + L"</TEXT><PARA/>";
		status += L"<POP/></RDL>";
		SendSetBaseInfoText2(iClientID, status);

		CheckIfResupplyingAvailable(carrierClientID, iClientID, Clients[iClientID].DockedToModule, SUPPLIES_INFO());

		// Disrupt autobuy.
		returncode = SKIPPLUGINS;
	}

	// Change last base for all docked ships if last base for carrier changes.
	vector<MODULE_CACHE> Modules = Clients[iClientID].DockedChars_Get();
	if (!Modules.empty())
	{
		uint baseSystemID = Universe::get_base(iBaseID)->iSystemID;
		for (vector<MODULE_CACHE>::iterator it = Modules.begin(); it != Modules.end(); ++it)
		{
			uint dockedClientID = HkGetClientIdFromCharname(it->occupiedBy);

			if (dockedClientID != -1 && !HkIsInCharSelectMenu(dockedClientID))
			{
				Clients[dockedClientID].saveLastBaseID = iBaseID;
				Clients[dockedClientID].saveLastPOBID = Clients[iClientID].POBID;

				if (Universe::get_base(iBaseID)->iSystemID != Players[dockedClientID].iSystemID)
				{
					JumpingDockedShips[dockedClientID] = true;
					ForceLandingClients[dockedClientID] = GetProxyBaseForCarrier(iClientID);
					POBForceLandingClients[dockedClientID] = Clients[iClientID].POBID;
					PrintUserCmdText(dockedClientID, L" ChangeSys %u", Players[dockedClientID].iSystemID);
				}
			}
		}
	}
}

void __stdcall PlayerLaunch(uint iShip, uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	// Check if client attempts to undock from carrier.
	OnlineData Data = Clients[iClientID];
	if (Data.DockedToModule && !Data.DockedWith.empty() && !ForceLandingClients[iClientID])
	{
		uint carrierClientID = HkGetClientIdFromCharname(Data.DockedWith);
		if (carrierClientID != -1 && !HkIsInCharSelectMenu(carrierClientID))
		{
			if (Players[carrierClientID].iBaseID)
			{
				ForceLandingClients[iClientID] = Players[carrierClientID].iBaseID;
				POBForceLandingClients[iClientID] = Clients[carrierClientID].POBID;
			}
			else
				ShipsToLaunch[iClientID] = iShip;
		}
		else
		{
			OfflineData carrierData = Clients[Data.DockedWith];
			if (carrierData.Location.baseID)
			{
				ForceLandingClients[iClientID] = carrierData.Location.baseID;
				POBForceLandingClients[iClientID] = carrierData.LastPOBID;
			}
			else
				ShipsToLaunch[iClientID] = iShip;
		}
	}
}

bool __stdcall Send_FLPACKET_SERVER_LAUNCH(uint iClientID, FLPACKET_LAUNCH& pLaunch)
{
	returncode = DEFAULT_RETURNCODE;

	// If the ship is present in ShipsToLaunch - handle it as undocking from carrier.
	if (pLaunch.iShip == ShipsToLaunch[iClientID])
	{
		OnlineData Data = Clients[iClientID];
		bool jumping = (mapDeferredJumps.find(iClientID) != mapDeferredJumps.end());

		Vector pos;
		Matrix ornt;
		Quaternion qt;

		uint carrierClientID = HkGetClientIdFromCharname(Data.DockedWith);
		if (carrierClientID != -1)
		{
			wstring charname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);

			if (!jumping)
			{
				Watcher.ReleaseModule(carrierClientID, charname);
				pub::SpaceObj::GetLocation(Players[carrierClientID].iShipID, pos, ornt);
				pub::Audio::PlaySoundEffect(carrierClientID, ID_sound_undocked);
			}
			else if (JettisoningClients[iClientID])
			{
				pub::Audio::PlaySoundEffect(carrierClientID, ID_sound_undocked);
				JettisoningClients[iClientID] = false;
			}
		}
		else
		{
			OfflineData carrierData = Clients[Data.DockedWith];
			pos = carrierData.Location.pos;
			ornt = EulerMatrix(carrierData.Location.rot);

			vector<MODULE_CACHE> &modules = carrierData.DockedChars;
			for (vector<MODULE_CACHE>::iterator it = modules.begin(); it != modules.end(); ++it)
			{
				if (it->occupiedBy == (wstring)(const wchar_t*)Players.GetActiveCharacterName(iClientID))
				{
					modules.erase(it);
					break;
				}
			}

			carrierData.Save();
		}

		if (!jumping)
		{
			qt = HkMatrixToQuaternion(ornt);
			uint undockDistance = Watcher.moduleArchInfo[Data.DockedToModule].undockDistance;
			pLaunch.fPos[0] = pos.x + ornt.data[0][1] * undockDistance;
			pLaunch.fPos[1] = pos.y + ornt.data[1][1] * undockDistance;
			pLaunch.fPos[2] = pos.z + ornt.data[2][1] * undockDistance;
			pLaunch.fRotate[0] = qt.w; pLaunch.fRotate[1] = qt.x; pLaunch.fRotate[2] = qt.y; pLaunch.fRotate[3] = qt.z;
		}

		Players[iClientID].iLastBaseID = Data.saveLastBaseID;
		Data.LastPOBID = Data.saveLastPOBID;
		ShipsToLaunch[iClientID] = 0;
		Data.DockedToModule = 0;
	}

	return true;
}

void __stdcall PlayerLaunch_AFTER(uint iShip, uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	// If ship requested docking earlier - cancel /dockatbase request.
	mapPendingDockingRequests[iClientID] = 0;

	if (ResupplyingClients[iClientID])
	{
		resupplyList.erase(remove_if(resupplyList.begin(), resupplyList.end(),
			[iClientID](const ActionResupply &action) { return action.dockedClientID == iClientID; }));

		ResupplyingClients[iClientID] = false;
	}

	if (ForceLandingClients[iClientID])
	{
		uint iSystemID = Players[iClientID].iSystemID;
		uint iBaseSystemID = Universe::get_base(ForceLandingClients[iClientID])->iSystemID;

		// If the ship should be landed on POB - do it through plugin communication.
		if (POBForceLandingClients[iClientID])
		{
			CUSTOM_BASE_BEAM_STRUCT message;
			message.bBeamed = false;
			message.iClientID = iClientID;
			message.iTargetBaseID = POBForceLandingClients[iClientID];
			Plugin_Communication(CUSTOM_BASE_BEAM, &message);
			POBForceLandingClients[iClientID] = 0;
		}
		else
		{
			pub::Player::ForceLand(iClientID, ForceLandingClients[iClientID]);
		}

		// Update player's system if base is in different system to avoid character-erasing bug.
		if (iBaseSystemID != iSystemID)
		{
			Server.BaseEnter(ForceLandingClients[iClientID], iClientID);
			Server.BaseExit(ForceLandingClients[iClientID], iClientID);
			wstring wscCharFileName;
			HkGetCharFileName(ARG_CLIENTID(iClientID), wscCharFileName);
			wscCharFileName += L".fl";
			CHARACTER_ID cID;
			strcpy(cID.szCharFilename, wstos(wscCharFileName.substr(0, 14)).c_str());
			Server.CharacterSelect(cID, iClientID);
		}

		// Ship is being redirected to proxy base in another system
		if (JumpingDockedShips[iClientID])
		{
			PrintUserCmdText(iClientID, L"Carrier has jumped.");
			JumpingDockedShips[iClientID] = false;
		}
		// Ship docks to carrier (proxy base)
		else if (mapDockingClients[iClientID])
		{
			uint carrierClientID = HkGetClientIdFromCharname(Clients[iClientID].DockedWith);
			pub::Audio::PlaySoundEffect(carrierClientID, ID_sound_docked);
			mapDockingClients[iClientID] = 0;
		}
		// Ship undocks from carrier at base
		else
		{
			OnlineData Data = Clients[iClientID];

			if (!Data.DockedWith.empty())
			{
				wstring charname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
				uint carrierClientID = HkGetClientIdFromCharname(Data.DockedWith);

				if (carrierClientID != -1 && !HkIsInCharSelectMenu(carrierClientID))
				{
					Watcher.ReleaseModule(carrierClientID, charname);
					pub::Audio::PlaySoundEffect(carrierClientID, ID_sound_undocked);
				}
				else
				{
					OfflineData carrierData = Clients[Data.DockedWith];

					vector<MODULE_CACHE> &modules = carrierData.DockedChars;
					for (vector<MODULE_CACHE>::iterator it = modules.begin(); it != modules.end(); ++it)
					{
						if (it->occupiedBy == charname)
						{
							modules.erase(it);
							break;
						}
					}

					carrierData.Save();
				}
			}

			Data.DockedToModule = 0;
		}

		ForceLandingClients[iClientID] = 0;
	}
}

void __stdcall ShipDestroyed(DamageList *_dmg, DWORD *ecx, uint kill)
{
	returncode = DEFAULT_RETURNCODE;

	CShip *cship = (CShip*)ecx[4];
	uint iClientID = cship->GetOwnerPlayer();

	if (iClientID)
	{
		// If the ship requested docking earlier - cancel request.
		CancelRequest(iClientID);

		// If the ship is carrier and docking queue is not empty - clear it.
		if (!dockingQueues[iClientID].empty())
		{
			vector<ActionDocking> &queue = dockingQueues[iClientID];
			for (ActionDocking &ship : queue)
				mapDockingClients[ship.dockingClientID] = 0;

			queue.clear();

			for (vector<uint>::iterator it = dockingList.begin(); it != dockingList.end(); ++it)
				if (*it == iClientID)
				{
					dockingList.erase(it);
					break;
				}
		}

		if (kill)
		{
			Vector pos = cship->get_position();
			Matrix ornt = cship->get_orientation();
			uint system = cship->iSystem;

			for (MODULE_CACHE &module : Watcher.Cache[iClientID].Modules)
			{
				if (module.occupiedBy.empty())
					continue;

				uint dockedClientID = HkGetClientIdFromCharname(module.occupiedBy);

				if (dockedClientID != -1 && HkIsInCharSelectMenu(dockedClientID))
				{
					// Kick the client, can't update data correctly while client is in character select menu.
					HkKick(ARG_CLIENTID(dockedClientID));
					dockedClientID = -1;
				}

				// Docked ship is online - move it to carrier's position
				if (dockedClientID != -1)
				{
					uint undockDistance = Watcher.moduleArchInfo[module.archID].undockDistance;
					Vector undockPos;

					undockPos.x = pos.x + ornt.data[0][1] * undockDistance;
					undockPos.y = pos.y + ornt.data[1][1] * undockDistance;
					undockPos.z = pos.z + ornt.data[2][1] * undockDistance;

					mapDeferredJumps[dockedClientID].system = system;
					mapDeferredJumps[dockedClientID].pos = undockPos;
					mapDeferredJumps[dockedClientID].ornt = ornt;

					PrintUserCmdText(dockedClientID, L" ChangeSys %u", system);
				}
				// Docked ship is offline - throw its cargo in space and move to last real base.
				else
				{
					OfflineData Data = Clients[module.occupiedBy];
					Data.Location.baseID = Data.saveLastBaseID;
					Data.Location.systemID = Universe::get_base(Data.saveLastBaseID)->iSystemID;
					Data.POBID = Data.saveLastPOBID;
					Data.DockedToModule = 0;

					for (const CARGO_ITEM &item : Data.Cargo.Get())
						Server.MineAsteroid(cship->iSystem, pos, ID_object_lootcrate, item.archID, item.count, iClientID);

					Data.Cargo.Clear();

					Data.Save();
				}

				module.occupiedBy.clear();
			}

			Clients[iClientID].DockedChars_Clear();
		}
	}
}

void __stdcall SystemSwitchOutComplete(uint iShip, uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	if (mapDeferredJumps.find(iClientID) != mapDeferredJumps.end())
		SwitchSystem(iClientID, iShip);

	// Handle system switch after carrier ship is moved to another system.
	if (Clients[iClientID].HasDockingModules)
		JumpingCarriers[iClientID] = true;
}

bool _stdcall Send_FLPACKET_SERVER_MISCOBJUPDATE_5(uint iClientID, uint iClientID2, uint iSystemID)
{
	returncode = DEFAULT_RETURNCODE;

	if (JumpingCarriers[iClientID2])
	{
		// If the ship has other ships docked inside - move them to new system.
		for (MODULE_CACHE &module : Clients[iClientID2].DockedChars_Get())
		{
			uint dockedClientID = HkGetClientIdFromCharname(module.occupiedBy);
			if (dockedClientID != -1 && !ForceLandingClients[dockedClientID] && !HkIsInCharSelectMenu(dockedClientID))
			{
				JumpingDockedShips[dockedClientID] = true;
				ForceLandingClients[dockedClientID] = GetProxyBaseForSystem(iClientID2, iSystemID);
				PrintUserCmdText(dockedClientID, L" ChangeSys %u", Players[dockedClientID].iSystemID);
			}
		}

		JumpingCarriers[iClientID2] = false;
	}

	return true;
}

void __stdcall DestroyCharacter(CHARACTER_ID const &cId, uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	wstring charname = HkGetCharnameFromCharFile(cId.szCharFilename, Players.FindAccountFromClientID(iClientID));
	OfflineData removingData = Clients[charname];

	if (removingData.DockedToModule)
	{
		// Remove the ship from carrier's module.
		uint carrierClientID = HkGetClientIdFromCharname(removingData.DockedWith);
		if (carrierClientID != -1 && HkIsInCharSelectMenu(iClientID))
		{
			Watcher.ReleaseModule(carrierClientID, charname);
			pub::Audio::PlaySoundEffect(carrierClientID, ID_sound_undocked);
		}
		else
		{
			OfflineData carrierData = Clients[removingData.DockedWith];

			remove_if(carrierData.DockedChars.begin(), carrierData.DockedChars.end(),
				[&charname](const MODULE_CACHE &module) { return module.occupiedBy == charname; });

			carrierData.Save();
		}
	}
	else if (!removingData.DockedChars.empty())
	{
		Vector &pos = removingData.Location.pos;
		Matrix ornt = EulerMatrix(removingData.Location.rot);
		uint system = removingData.Location.systemID;
		uint base = removingData.Location.baseID;

		
		for (MODULE_CACHE &module : removingData.DockedChars)
		{
			if (module.occupiedBy.empty())
				continue;

			uint dockedClientID = HkGetClientIdFromCharname(module.occupiedBy);

			if (dockedClientID != -1 && HkIsInCharSelectMenu(dockedClientID))
			{
				// Kick the client, can't update data correctly while client is in character select menu.
				HkKick(ARG_CLIENTID(dockedClientID));
				dockedClientID = -1;
			}

			// Move it to carrier's position.
			if (dockedClientID != -1)
			{
				if (base)
				{
					ForceLandingClients[dockedClientID] = base;
					POBForceLandingClients[dockedClientID] = removingData.LastPOBID;
					PrintUserCmdText(dockedClientID, L" ChangeSys %u", system);
				}
				else
				{
					uint undockDistance = Watcher.moduleArchInfo[module.archID].undockDistance;
					Vector undockPos;

					undockPos.x = pos.x + ornt.data[0][1] * undockDistance;
					undockPos.y = pos.y + ornt.data[1][1] * undockDistance;
					undockPos.z = pos.z + ornt.data[2][1] * undockDistance;

					mapDeferredJumps[dockedClientID].system = system;
					mapDeferredJumps[dockedClientID].pos = undockPos;
					mapDeferredJumps[dockedClientID].ornt = ornt;

					PrintUserCmdText(dockedClientID, L" ChangeSys %u", system);
				}

				Clients[dockedClientID].DockedWith = wstring(L"");
				Clients[dockedClientID].DockedToModule = 0;
			}
			else
			{
				OfflineData dockedData = Clients[module.occupiedBy];

				if (base)
				{
					dockedData.Location.baseID = base;
					dockedData.POBID = removingData.LastPOBID;
					dockedData.Location.systemID = system;
					dockedData.DockedToModule = 0;
				}
				else
				{
					dockedData.Location.pos = pos;
					dockedData.Location.rot = removingData.Location.rot;
					dockedData.Location.systemID = system;
					dockedData.Location.baseID = 0;
					dockedData.POBID = 0;
					dockedData.DockedToModule = 0;
				}

				dockedData.Save();
			}
		}
	}
}

void __stdcall ReqHullStatus(float reqRelativeHealth, uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	if (Clients[iClientID].DockedToModule)
	{
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;

		float maxHitPoints = Archetype::GetShip(Players[iClientID].iShipArchetype)->fHitPoints;
		float diff = reqRelativeHealth - Players[iClientID].fRelativeHealth;

		pub::Player::AdjustCash(iClientID, (int)floor(maxHitPoints * diff / 100 * 33));

		uint carrierClientID = HkGetClientIdFromCharname(Clients[iClientID].DockedWith);
		if (carrierClientID != -1 && !HkIsInCharSelectMenu(carrierClientID))
		{
			ID_TRAITS &traits = Watcher.Cache[carrierClientID].dockingTraits;

			int toRepair = (int)floor(maxHitPoints * diff);
			int toRepairPrevious = toRepair;

			EQ_ITEM *item;
			traverse_equipment(carrierClientID, item)
			{
				if (toRepair <= 0)
					break;

				auto supplyIter = traits.supplyItems.find(item->iArchID);
				if (supplyIter != traits.supplyItems.end())
				{
					SUPPLY &supply = supplyIter->second;
					if (supply.hullPerUnit)
					{
						ushort efficiency = supply.hullPerUnit;
						int toUse = 0;
						int toAdd = item->iCount * efficiency;

						if (toRepair < toAdd)
							toAdd = toRepair;

						toRepair -= toAdd;
						toUse = (int)ceil(toAdd / efficiency);
						HkRemoveCargo(ARG_CLIENTID(carrierClientID), item->sID, toUse);
					}
				}

				continue_traverse(item);
			}

			if (toRepair < 0)
				toRepair = 0;

			if (toRepair == toRepairPrevious)
			{
				PrintUserCmdText(iClientID, L"Carrier ship has no components to provide hull repairment service.");
			}
			else
			{
				// Would be great if reqRelativeHealth was passed by reference.
				GetClientInterface()->Send_FLPACKET_SERVER_SETHULLSTATUS(iClientID, 1 - toRepair / maxHitPoints);
				Players[iClientID].fRelativeHealth = 1 - toRepair / maxHitPoints;
			}
		}
		else
		{
			PrintUserCmdText(iClientID, L"Can't repair when pilot is not present.");
		}
	}
}

typedef bool(*_UserCmdProc)(uint, const wstring &);

struct USERCMD
{
	wchar_t *wszCmd;
	_UserCmdProc proc;
};

// The user chat commands for this plugin
USERCMD UserCmds[] =
{
	{ L"/listdocked",		Commands::Listdocked		},
	{ L"/conn",				Commands::Conn				},
	{ L"/return",			Commands::Return			},
	{ L"/renameme",			Commands::Renameme			},
	{ L"/jettisonship",		Commands::Jettisonship		},
	{ L"/jettisonallships",	Commands::Jettisonallships	},
	{ L"/allowdock",		Commands::Allowdock			},
	{ L"/dockatbase",		Commands::Dockatbase		},
	{ L"/loadsupplies",		Commands::Loadsupplies		},
};


bool UserCmd_Process(uint iClientID, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	wstring wscCmdLineLower = ToLower(wscCmd);

	// Scan for exact beginning match with one of our commands.
	for (uint i = 0; (i < sizeof(UserCmds) / sizeof(USERCMD)); ++i)
	{
		if (boost::algorithm::starts_with(wscCmdLineLower, UserCmds[i].wszCmd))
		{
			// Dispatch the command to the appropriate processing function.
			if (UserCmds[i].proc(iClientID, wscCmd))
			{
				// If handled command returns true - stop further handling.
				returncode = SKIPPLUGINS_NOFUNCTIONCALL;
			}

			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions exported for FLHook
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

	return true;
}

EXPORT PLUGIN_RETURNCODE Get_PluginReturnCode()
{
	return returncode;
}

EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "Docking Modules by Invoker";
	p_PI->sShortName = "specdock";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 3));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Timers::HkTimerCheckKick, PLUGIN_HkTimerCheckKick, 0));

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Dock_Call, PLUGIN_HkCb_Dock_Call, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&RequestCancel, PLUGIN_HkIServerImpl_RequestCancel, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ReqShipArch_AFTER, PLUGIN_HkIServerImpl_ReqShipArch_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseEnter_AFTER, PLUGIN_HkIServerImpl_BaseEnter_AFTER, 1));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch, PLUGIN_HkIServerImpl_PlayerLaunch, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Send_FLPACKET_SERVER_LAUNCH, PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_LAUNCH, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch_AFTER, PLUGIN_HkIServerImpl_PlayerLaunch_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ShipDestroyed, PLUGIN_ShipDestroyed, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SystemSwitchOutComplete, PLUGIN_HkIServerImpl_SystemSwitchOutComplete, 3));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Send_FLPACKET_SERVER_MISCOBJUPDATE_5, PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_MISCOBJUPDATE_5, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&DestroyCharacter, PLUGIN_HkIServerImpl_DestroyCharacter, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ReqHullStatus, PLUGIN_HkIServerImpl_ReqHullStatus, 0));

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ModuleWatcher::CharacterSelect_AFTER, PLUGIN_HkIServerImpl_CharacterSelect_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ModuleWatcher::ReqEquipment_AFTER, PLUGIN_HkIServerImpl_ReqEquipment_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ModuleWatcher::ReqAddItem, PLUGIN_HkIServerImpl_ReqAddItem, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ModuleWatcher::ReqAddItem_AFTER, PLUGIN_HkIServerImpl_ReqAddItem_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ModuleWatcher::ReqRemoveItem, PLUGIN_HkIServerImpl_ReqRemoveItem, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ModuleWatcher::SPScanCargo_AFTER, PLUGIN_HkIServerImpl_SPScanCargo_AFTER, 0));

	return p_PI;
}