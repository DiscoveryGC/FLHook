#include "Main.h"

bool CMD_listdocked(uint client, const wstring& wscCmd)
{
	PrintUserCmdText(client, EnumerateDockedShips(client));
	return true;
}

bool CMD_conn(uint client, const wstring& wscCmd)
{
	// This plugin always runs before the Conn Plugin runs it's /conn function. Verify that there are no docked ships.
	if (!mobiledockClients[client].dockedShips.empty())
	{
		PrintUserCmdText(client, L"You cannot use this command if you have vessels docked with you!");
		return false;
	}

	// Verify that client is not currently docked to carrier.
	if (mobiledockClients[client].proxyBaseID != 0 && mobiledockClients[client].proxyBaseID != -1)
	{
		PrintUserCmdText(client, L"Connecticut virtual reality module is not found on that base.");
		return false;
	}

	return true;
}

bool CMD_return(uint client, const wstring& wscCmd)
{
	// This plugin always runs before the Conn Plugin runs it's /return function. Verify that there are no docked ships.
	if (!mobiledockClients[client].dockedShips.empty())
	{
		PrintUserCmdText(client, L"You cannot use this command if you have vessels docked with you!");
		return false;
	}

	return true;
}

bool CMD_renameme(uint client, const wstring& wscCmd)
{
	// I was too tired to write handler for that. Just restricted that.
	if (mobiledockClients[client].proxyBaseID != 0 && mobiledockClients[client].proxyBaseID != -1)
	{
		PrintUserCmdText(client, L"You cannot use this command if you are docked to someone!");
		return false;
	}
	else if (!mobiledockClients[client].dockedShips.empty())
	{
		PrintUserCmdText(client, L"You cannot use this command if you have vessels docked to you!");
		return false;
	}

	return true;
}

bool CMD_jettisonship(uint client, const wstring& wscCmd)
{
	// Get the supposed ship we should be ejecting from the command parameters.
	wstring dockedCharname = Trim(GetParam(wscCmd, ' ', 1));
	if (dockedCharname.empty())
	{
		PrintUserCmdText(client, L"Usage: /jettisonship <charname>");
		return true;
	}

	// Check to see if the user listed is actually docked with the carrier at the moment.
	vector<wstring> &dockedShips = mobiledockClients[client].dockedShips;
	if (find(dockedShips.begin(), dockedShips.end(), dockedCharname) == dockedShips.end())
	{
		PrintUserCmdText(client, L"%s is not docked with you!", dockedCharname);
		return true;
	}

	// The player exists. Remove him from the docked list, and kick him into space.
	JettisonShip(client, dockedCharname);

	return true;
}

bool CMD_jettisonallships(uint client, const wstring& wscCmd)
{
	if (mobiledockClients[client].dockedShips.empty())
	{
		PrintUserCmdText(client, L"You have no ships on board.");
	}
	else
	{
		for (uint i = 0; i < mobiledockClients[client].dockedShips.size(); i++)
		{
			wstring dockedCharname = mobiledockClients[client].dockedShips[i];
			JettisonShip(client, dockedCharname);
		}
	}

	return true;
}

bool CMD_allowdock(uint client, const wstring& wscCmd)
{
	// Check if it is base-docking.
	wstring targetName = Trim(GetParam(wscCmd, ' ', 1));
	if (!targetName.empty())
	{
		uint iTargetClientID = HkGetClientIdFromCharname(targetName);
		if (mapPendingDockingRequests[iTargetClientID] == client)
		{
			PrintUserCmdText(client, L"You allowed %s to dock", targetName);
			pub::Audio::PlaySoundEffect(iTargetClientID, CreateID("sfx_ui_react_processing01"));
			PrintUserCmdText(iTargetClientID, L"Docking request accepted. Launch your ship to move it to carrier.");
			mobiledockClients[iTargetClientID].wscDockedWithCharname = (const wchar_t*)Players.GetActiveCharacterName(client);
			mobiledockClients[iTargetClientID].iLastBaseID = -1;
			return true;
		}
	}

	// If we're not in space, then ignore the request.
	uint iShip;
	pub::Player::GetShip(client, iShip);
	if (!iShip)
		return true;

	// If there is no ship currently targeted, then ignore the request.
	uint iTargetShip;
	pub::SpaceObj::GetTarget(iShip, iTargetShip);
	if (!iTargetShip)
		return true;

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

	// Find the docking request. If none, ignore.
	if (mapPendingDockingRequests.find(iTargetClientID) == mapPendingDockingRequests.end())
	{
		PrintUserCmdText(client, L"No pending docking requests for this ship");
		return true;
	}

	// Check that there is an empty docking module.
	if (mobiledockClients[client].iDockingModulesInstalled - mobiledockClients[client].dockedShips.size() == 0)
	{
		PrintUserCmdText(client, L"No free docking modules available.");
		return true;
	}

	DockShip(iShip, client, iTargetClientID);
	return true;
}

bool CMD_dockatbase(uint client, const wstring& wscCmd)
{
	wstring charname = Trim(GetParam(wscCmd, ' ', 1));
	uint iTargetClientID = HkGetClientIdFromCharname(charname);
	Archetype::Ship *ship = Archetype::GetShip(Players[client].iShipArchetype);

	if (charname.empty())
	{
		PrintUserCmdText(client, L"Usage: /dockatbase <charname>");
		return true;
	}
	else if (Players[client].iBaseID == 0)
	{
		PrintUserCmdText(client, L"You cannot use this command in space!");
		return true;
	}
	else if (iTargetClientID == -1)
	{
		PrintUserCmdText(client, L"Character was not found at server.");
		return true;
	}
	else if (iTargetClientID == client)
	{
		PrintUserCmdText(client, L"You cannot use this command on yourself!");
		return true;
	}
	else if (Players[iTargetClientID].iBaseID != Players[client].iBaseID)
	{
		PrintUserCmdText(client, L"Ship is out of range.");
		return true;
	}
	else if (UpdateAvailableModules(iTargetClientID) - mobiledockClients[iTargetClientID].dockedShips.size() == 0)
	{
		PrintUserCmdText(client, L"Target ship has no free docking capacity");
		return true;
	}
	else if (ship->fHoldSize > cargoCapacityLimit)
	{
		PrintUserCmdText(client, L"Target ship cannot dock a ship of your size.");
		return true;
	}
	else if (Players[client].iBaseID == GetProxyBaseForClient(client))
	{
		PrintUserCmdText(client, L"Docking to carrier at player bases is not allowed.");
		return true;
	}
	else
	{
		uint iTargetClientID = HkGetClientIdFromCharname(charname);

		// Check if we already have docking request from this ship. If so - cancel it.
		if (mapPendingDockingRequests.find(client) != mapPendingDockingRequests.end())
		{
			if (mapPendingDockingRequests[client] == iTargetClientID)
			{
				pub::Audio::PlaySoundEffect(client, CreateID("sfx_ui_react_accept01"));
				PrintUserCmdText(client, L"Docking request was canceled.");
				mapPendingDockingRequests.erase(client);
				return true;
			}
			else
			{
				PrintUserCmdText(client, L"Docking request to previous ship was canceled.");
				mapPendingDockingRequests.erase(client);
			}
		}

		// Check if both ships are in same group.
		bool IsInGroupWithCarrier = false;
		pub::Player::IsGroupMember(client, iTargetClientID, IsInGroupWithCarrier);
		if (IsInGroupWithCarrier)
		{
			pub::Audio::PlaySoundEffect(client, CreateID("sfx_ui_react_processing01"));
			PrintUserCmdText(client, L"Docking request accepted. Launch your ship to move it to carrier.");
			mobiledockClients[client].wscDockedWithCharname = charname;
			mobiledockClients[client].iLastBaseID = -1;
		}
		else
		{
			PrintUserCmdText(client, L"Docking request sent to %s. Player must authorize you with /allowdock command or you must be in same group.", Players.GetActiveCharacterName(iTargetClientID));
			mapPendingDockingRequests[client] = iTargetClientID;
		}
		return true;
	}
}

bool CMD_loadsupplies(uint client, const wstring& wscCmd)
{
	if (mobiledockClients[client].wscDockedWithCharname.empty())
		return true;

	wstring dockedCharname = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(client));
	wstring carrierCharname = mobiledockClients[client].wscDockedWithCharname;
	uint carrierClientID = HkGetClientIdFromCharname(carrierCharname);

	if (carrierClientID != -1)
	{
		Archetype::Ship *ship = Archetype::GetShip(Players[client].iShipArchetype);

		map<uint, int> ammoInCart;
		int cloakBatteriesInCart = 0;
		int nanobotsInCart = ship->iMaxNanobots;
		int batteriesInCart = ship->iMaxShieldBats;
		int totalAmmoInCart = 0;

		map<uint, int> ammo;
		map<uint, int> guns;
		uint cloak = 0;

		// Check what player already has.
		list<CARGO_INFO> lstCargo;
		int iRemainingHoldSize = 0;
		HkEnumCargo(dockedCharname, lstCargo, iRemainingHoldSize);

		foreach(lstCargo, CARGO_INFO, item)
		{
			// Check if item present in plugin gun database.
			if(item->bMounted == true && mapAmmo.find(item->iArchID) != mapAmmo.end())
			{
				if (mapAmmo.find(item->iArchID) != mapAmmo.end())
				{
					if (guns.find(item->iArchID) == guns.end())
						guns[item->iArchID] = 1;
					else if (mapAmmo[item->iArchID].stackable)
						guns[item->iArchID]++;
				}
			}
			// Check if item present in plugin cloaking devices database.
			if (mapBatteries.find(item->iArchID) != mapBatteries.end())
			{
				cloak = item->iArchID;
			}
			// Check if item present in plugin ammo database.
			else if (item->bMounted == false)
			{
				// Check for usual ammo.
				for (map<uint, AMMO>::iterator it = mapAmmo.begin(); it != mapAmmo.end(); it++)
				{
					if (it->second.ammoID == item->iArchID)
					{
						ammo[item->iArchID] = item->iCount;
					}
				}
				// Check for cloaking device ammo.
				for (map<uint, uint>::iterator it = mapBatteries.begin(); it != mapBatteries.end(); it++)
				{
					if (it->second == item->iArchID)
					{
						ammo[item->iArchID] = item->iCount;
					}
				}
				if (item->iArchID == batteriesID)
				{
					batteriesInCart = ship->iMaxShieldBats - item->iCount;
				}
				if (item->iArchID == nanobotsID)
				{
					nanobotsInCart = ship->iMaxNanobots - item->iCount;
				}
			}
		}

		// Calculate how many this player needs.
		for (map<uint, int>::iterator it = guns.begin(); it != guns.end(); it++)
		{
			uint ammoID = mapAmmo[it->first].ammoID;

			int need = mapAmmo[it->first].ammoLimit * it->second;
			if (ammo.find(ammoID) != ammo.end())
				need -= ammo[ammoID];

			totalAmmoInCart += need;
			ammoInCart[ammoID] = need;
		}

		if(cloak)
		{
			uint ammoID = mapBatteries[cloak];

			int need = 1000;
			if (ammo.find(ammoID) != ammo.end())
				need -= ammo[ammoID];

			cloakBatteriesInCart = need;
		}

		if (totalAmmoInCart + cloakBatteriesInCart + batteriesInCart + nanobotsInCart == 0)
		{
			PrintUserCmdText(client, L"Resupplying is not needed.");
			return true;
		}
		if (Players[carrierClientID].iBaseID && Players[carrierClientID].iBaseID != GetProxyBaseForClient(carrierClientID))
		{
			PrintUserCmdText(client, L"Go buy some ammo at base. Don't waste carrier's supplies.");
			return true;
		}

		// Calculate time needed for resupplying. Return if carrier can't provide resupplying and notify docked ship about that.
		SUPPLY_INFO info = SUPPLY_INFO();
		int time = CheckIfResupplyingAvailable(carrierClientID, client, info, false);
		if (time == -1) return true;

		// Check if carrier is lacking specific supplies and warn docked ship about that.
		if (info.hasAmmoSup == false && totalAmmoInCart != 0)
		{
			totalAmmoInCart = 0;
			if (totalAmmoInCart + cloakBatteriesInCart + batteriesInCart + nanobotsInCart != 0)
				PrintUserCmdText(client, L"Warning. Carrier has no ammo for you.");
		}

		if (info.hasCloakSup == false && cloakBatteriesInCart != 0)
		{
			cloakBatteriesInCart = 0;
			if (totalAmmoInCart + cloakBatteriesInCart + batteriesInCart + nanobotsInCart != 0)
				PrintUserCmdText(client, L"Warning. Carrier has no batteries for your cloaking device.");
		}

		if (info.hasHullSup == false && nanobotsInCart != 0)
		{
			nanobotsInCart = 0;
			if (totalAmmoInCart + cloakBatteriesInCart + batteriesInCart + nanobotsInCart != 0)
				PrintUserCmdText(client, L"Warning. Carrier has no nanobots for your ship.");
		}

		if (info.hasShieldSup == false && batteriesInCart != 0)
		{
			batteriesInCart = 0;
			if (totalAmmoInCart + cloakBatteriesInCart + batteriesInCart + nanobotsInCart != 0)
				PrintUserCmdText(client, L"Warning. Carrier has no shield batteries for your ship.");
		}

		if (totalAmmoInCart + cloakBatteriesInCart + batteriesInCart + nanobotsInCart == 0)
		{
			PrintUserCmdText(client, L"Resupplying is not possible. Carrier has no requested supplies in cargo hold.");
			return true;
		}

		pub::Audio::PlaySoundEffect(client, CreateID("sfx_ui_react_processing01"));
		PrintUserCmdText(client, L"Resupplying begins. Wait %i seconds before work is finished.", time);

		// Start timer.
		TaskScheduler.push_back(Task(time, [carrierCharname, dockedCharname, time, ammoInCart, cloakBatteriesInCart, nanobotsInCart, batteriesInCart, totalAmmoInCart, cloak]()
		{
			map<uint, int> iAmmoInCart = ammoInCart;
			int iCloakBatteriesInCart = cloakBatteriesInCart;
			int iNanobotsInCart = nanobotsInCart;
			int iBatteriesInCart = batteriesInCart;
			int iTotalAmmoInCart = totalAmmoInCart;

			// Check if both are still at server.
			uint carrierClientId = HkGetClientIdFromCharname(carrierCharname);
			uint dockedClientID = HkGetClientIdFromCharname(dockedCharname);

			if (dockedClientID == -1)
				return;

			// Check if ship is still docked.
			if (mobiledockClients[dockedClientID].wscDockedWithCharname.empty())
				return;

			if (carrierClientId == -1)
			{
				PrintUserCmdText(dockedClientID, L"Carrier pilot went to bathroom and does not respond. Resupplying disrupted.");
				return;
			}

			// Check if carrier still has supplies.
			SUPPLY_INFO checkinfo = SUPPLY_INFO();
			if (CheckIfResupplyingAvailable(carrierClientId, dockedClientID, checkinfo, false) == -1)
				return;

			// Check if carrier still at base.
			if (Players[carrierClientId].iBaseID && Players[carrierClientId].iBaseID != GetProxyBaseForClient(carrierClientId))
			{
				PrintUserCmdText(dockedClientID, L"Go buy some ammo at base. Don't waste carrier's supplies.");
				return;
			}

			// Start resupplying.
			list<CARGO_INFO> listCargo;
			int remainingHoldSize = 0;
			HkEnumCargo(carrierCharname, listCargo, remainingHoldSize);

			foreach(listCargo, CARGO_INFO, item)
			{
				if (mapSupplies.find(item->iArchID) != mapSupplies.end())
				{
					int efficiency = mapSupplies[item->iArchID].efficiency;
					int type = mapSupplies[item->iArchID].type;

					if (type == 1 && iTotalAmmoInCart != 0)
					{
						int toUse = 0;
						int toAdd = item->iCount * efficiency;
						int boost = 1;

						for (map<uint, int>::const_iterator it = iAmmoInCart.begin(); it != iAmmoInCart.end(); it++)
						{
							if(find(boostedAmmo.begin(), boostedAmmo.end(), it->first) != boostedAmmo.end())
							{
								boost = 20;
								toAdd = toAdd * boost;
							}
							else if (boost > 1)
							{
								toAdd = toAdd / boost;
								boost = 1;
							}

							int ammotoAdd = 0;
							if (it->second <= toAdd)
							{
								ammotoAdd = it->second;
								iAmmoInCart.erase(it->first);
							}
							else if (toAdd != 0)
							{
								ammotoAdd = toAdd;
								iAmmoInCart[it->first] -= ammotoAdd;
							}
							else
							{
								break;
							}
							toAdd -= ammotoAdd;
							toUse += ammotoAdd / efficiency / boost;
							iTotalAmmoInCart -= ammotoAdd;

							HkAddCargo(ARG_CLIENTID(dockedClientID), it->first, ammotoAdd, false);
						}

						HkRemoveCargo(ARG_CLIENTID(carrierClientId), item->iID, toUse);
					}
					else if (type == 2 && iBatteriesInCart + iNanobotsInCart != 0)
					{
						int toUse = item->iCount;
						int toAdd = item->iCount * efficiency;
						int batstoAdd = 0;
						int botstoAdd = 0;
						if (iBatteriesInCart + iNanobotsInCart <= toAdd)
						{
							batstoAdd = iBatteriesInCart;
							botstoAdd = iNanobotsInCart;
							toUse = iBatteriesInCart / efficiency + iNanobotsInCart / efficiency;
						}
						else
						{
							if (iBatteriesInCart < toAdd)
							{
								batstoAdd = iBatteriesInCart;
								toAdd -= iBatteriesInCart;
								botstoAdd = toAdd;
							}
							else
							{
								batstoAdd = toAdd;
							}
						}

						if (toUse == 0)
							toUse = 1;

						iBatteriesInCart -= batstoAdd;
						iNanobotsInCart -= botstoAdd;

						if (batstoAdd)
							HkAddCargo(ARG_CLIENTID(dockedClientID), batteriesID, batstoAdd, false);
						if (botstoAdd)
							HkAddCargo(ARG_CLIENTID(dockedClientID), nanobotsID, botstoAdd, false);
						HkRemoveCargo(ARG_CLIENTID(carrierClientId), item->iID, toUse);
					}
					else if (type == 3 && iBatteriesInCart != 0)
					{
						int toUse = item->iCount;
						int toAdd = item->iCount * efficiency;
						if (iBatteriesInCart < toAdd)
						{
							toAdd = iBatteriesInCart;
							toUse = iBatteriesInCart / efficiency;
							if (toUse == 0)
								toUse = 1;
						}

						iBatteriesInCart -= toAdd;

						HkAddCargo(ARG_CLIENTID(dockedClientID), batteriesID, toAdd, false);
						HkRemoveCargo(ARG_CLIENTID(carrierClientId), item->iID, toUse);
					}
					else if (type == 4 && iNanobotsInCart != 0)
					{
						int toUse = item->iCount;
						int toAdd = item->iCount * efficiency;
						if (iNanobotsInCart < toAdd)
						{
							toAdd = iNanobotsInCart;
							toUse = iNanobotsInCart / efficiency;
							if (toUse == 0)
								toUse = 1;
						}

						iNanobotsInCart -= toAdd;

						HkAddCargo(ARG_CLIENTID(dockedClientID), nanobotsID, toAdd, false);
						HkRemoveCargo(ARG_CLIENTID(carrierClientId), item->iID, toUse);
					}
					else if (type == 5 && iCloakBatteriesInCart != 0)
					{
						int toUse = item->iCount;
						int toAdd = item->iCount * efficiency;
						if (iCloakBatteriesInCart < toAdd)
						{
							toAdd = iCloakBatteriesInCart;
							toUse = iCloakBatteriesInCart / efficiency;
							if (toUse == 0)
								toUse = 1;
						}

						iCloakBatteriesInCart -= toAdd;

						HkAddCargo(ARG_CLIENTID(dockedClientID), mapBatteries[cloak], toAdd, false);
						HkRemoveCargo(ARG_CLIENTID(carrierClientId), item->iID, toUse);
					}
					else if (type == 0)
					{
						int toUse = 0;
						int toAdd = item->iCount * efficiency;
						if (iTotalAmmoInCart != 0)
						{
							int boost = 1;

							for (map<uint, int>::const_iterator it = iAmmoInCart.begin(); it != iAmmoInCart.end(); it++)
							{
								if (find(boostedAmmo.begin(), boostedAmmo.end(), it->first) != boostedAmmo.end())
								{
									boost = 20;
									toAdd = toAdd * boost;
								}
								else if (boost > 1)
								{
									toAdd = toAdd / boost;
									boost = 1;
								}

								int ammotoAdd = 0;
								if (it->second <= toAdd)
								{
									ammotoAdd = it->second;
									iAmmoInCart.erase(it->first);
								}
								else if (toAdd != 0)
								{
									ammotoAdd = toAdd;
									iAmmoInCart[it->first] -= ammotoAdd;
								}
								else
								{
									break;
								}
								toAdd -= ammotoAdd;
								toUse += ammotoAdd / efficiency / boost;
								iTotalAmmoInCart -= ammotoAdd;

								HkAddCargo(ARG_CLIENTID(dockedClientID), it->first, ammotoAdd, false);
							}

							if (boost > 1)
							{
								toAdd = toAdd / boost;
								boost = 1;
							}
						}

						if (iBatteriesInCart + iNanobotsInCart != 0)
						{
							int batstoAdd = 0;
							int botstoAdd = 0;
							if (iBatteriesInCart + iNanobotsInCart <= toAdd)
							{
								batstoAdd = iBatteriesInCart;
								botstoAdd = iNanobotsInCart;
								toUse += (iBatteriesInCart + iNanobotsInCart) / efficiency;
							}
							else
							{
								toUse += toAdd / efficiency;
								if (iBatteriesInCart < toAdd)
								{
									batstoAdd = iBatteriesInCart;
									toAdd -= iBatteriesInCart;
									botstoAdd = toAdd;
								}
								else
								{
									batstoAdd = toAdd;
								}
							}

							iBatteriesInCart -= batstoAdd;
							iNanobotsInCart -= botstoAdd;


							if (batstoAdd)
								HkAddCargo(ARG_CLIENTID(dockedClientID), batteriesID, batstoAdd, false);
							if (botstoAdd)
								HkAddCargo(ARG_CLIENTID(dockedClientID), nanobotsID, botstoAdd, false);
						}

						HkRemoveCargo(ARG_CLIENTID(carrierClientId), item->iID, toUse);
					}
				}
			}

			pub::Audio::PlaySoundEffect(dockedClientID, CreateID("depot_open_sound"));
			PrintUserCmdText(dockedClientID, L"Resupplying complete.", time);
		}));
	}
	else
	{
		PrintUserCmdText(client, L"Carrier pilot went to bathroom and does not respond. Resupplying disabled.");
	}

	return true;
}