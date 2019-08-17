#include "Main.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Helper functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

ErrorMessage TryDockAtBase(vector<MODULE_CACHE> &Modules, uint dockingClientID, uint carrierClientID, wstring dockingName)
{
	if (Modules.empty())
		return NO_MODULES;

	ErrorMessage errorMsg = TOO_LARGE;
	float holdSize = Archetype::GetShip(Players[dockingClientID].iShipArchetype)->fHoldSize;

	for (vector<MODULE_CACHE>::iterator it = Modules.begin(); it != Modules.end(); it++)
	{
		MODULE_ARCH module = Watcher.moduleArchInfo[it->archID];

		if (holdSize <= module.maxCargoCapacity)
		{
			errorMsg = NO_FREE_MODULES;
			if (it->occupiedBy.empty())
			{
				bool IsInGroupWithCarrier = false;
				pub::Player::IsGroupMember(dockingClientID, carrierClientID, IsInGroupWithCarrier);

				if (IsInGroupWithCarrier || mapPendingDockingRequests[dockingClientID] == carrierClientID)
				{
					OnlineData Data = Clients[dockingClientID];
					Data.saveLastBaseID = Players[dockingClientID].iLastBaseID;
					Data.saveLastPOBID = Clients[dockingClientID].POBID;
					Data.DockedWith = (wstring)(const wchar_t*)Players.GetActiveCharacterName(carrierClientID);
					Data.DockedToModule = it->archID;

					it->occupiedBy = dockingName;
					Clients[carrierClientID].DockedChars.Add(*it);

					PrintUserCmdText(dockingClientID, L"Request accepted. Docking immediately.");
					mapDockingClients[dockingClientID] = carrierClientID;
					PrintUserCmdText(dockingClientID, L" ChangeSys %u", Players[carrierClientID].iSystemID);
					ForceLandingClients[dockingClientID] = GetProxyBaseForCarrier(carrierClientID);

					if (!IsInGroupWithCarrier)
						PrintUserCmdText(carrierClientID, L"You allowed " + dockingName + L" to dock.");
				}
				else
				{
					mapPendingDockingRequests[dockingClientID] = carrierClientID;
					PrintUserCmdText(dockingClientID, L"Docking request sent to %s. Player must authorize you with /allowdock command or you must be in same group.", Players.GetActiveCharacterName(carrierClientID));
				}

				return OK;
			}
		}
	}

	return errorMsg;
}

ErrorMessage TryDockInSpace(vector<MODULE_CACHE> &Modules, uint dockingClientID, uint carrierClientID, uint dockingShip, uint carrierShip, wstring dockingName)
{
	if (Modules.empty())
		return NO_MODULES;

	// Check if there is place in queue.
	vector<MODULE_CACHE> checkModules = Modules;
	for (vector<ActionDocking>::iterator it = dockingQueues[carrierClientID].begin(); it != dockingQueues[carrierClientID].end(); it++)
	{
		for (vector<MODULE_CACHE>::iterator mit = checkModules.begin(); mit != checkModules.end(); mit++)
		{
			if (mit->occupiedBy.empty() && it->moduleArch == mit->archID)
			{
				checkModules.erase(mit);
				break;
			}
		}
	}

	ErrorMessage errorMsg = TOO_LARGE;

	float holdSize = Archetype::GetShip(Players[dockingClientID].iShipArchetype)->fHoldSize;
	float distance = HkDistance3DByShip(dockingShip, carrierShip);

	// For each module check if it is suitable for the ship.
	for (vector<MODULE_CACHE>::iterator it = checkModules.begin(); it != checkModules.end(); it++)
	{
		MODULE_ARCH module = Watcher.moduleArchInfo[it->archID];

		if (holdSize <= module.maxCargoCapacity)
		{
			errorMsg = NO_FREE_MODULES;
			if (it->occupiedBy.empty())
			{
				errorMsg = TOO_FAR;
				if (distance <= module.dockDisatnce)
				{
					bool IsInGroupWithCarrier;
					pub::Player::IsGroupMember(dockingClientID, carrierClientID, IsInGroupWithCarrier);

					if (IsInGroupWithCarrier || mapPendingDockingRequests[dockingClientID] == carrierClientID)
					{
						DelayedDocking(dockingClientID, carrierClientID, it->archID, module.dockDisatnce, module.dockingTime);
						PrintUserCmdText(dockingClientID, L"Request accepted. Docking in %u seconds.", module.dockingTime);

						if (IsInGroupWithCarrier)
							MiscCmds::ExportSetLights(dockingClientID);
						else
							mapPendingDockingRequests[dockingClientID] = 0;

					}
					else
					{
						MiscCmds::ExportSetLights(dockingClientID);
						mapPendingDockingRequests[dockingClientID] = carrierClientID;
						PrintUserCmdText(dockingClientID, L"Docking request sent to %s. Player must authorize you with /allowdock command or you must be in same group.", Players.GetActiveCharacterName(carrierClientID));
					}

					return OK;
				}
			}
		}
	}

	return errorMsg;
}

void CancelRequest(uint dockingClientID)
{
	// If ship requested docking earlier - cancel request.
	if (mapPendingDockingRequests[dockingClientID] != 0)
	{
		MiscCmds::ExportSetLights(dockingClientID);
		mapPendingDockingRequests[dockingClientID] = 0;
	}

	// If ship was in docking queue - erase from the queue to allow other ships to dock.
	if (mapDockingClients[dockingClientID] != 0)
	{
		MiscCmds::ExportSetLights(dockingClientID);
		vector<ActionDocking> &queue = dockingQueues[mapDockingClients[dockingClientID]];
		for (vector<ActionDocking>::iterator it = queue.begin(); it != queue.end(); it++)
		{
			if (it->dockingClientID == dockingClientID)
			{
				queue.erase(it);
				break;
			}
		}

		mapDockingClients[dockingClientID] = 0;
	}
}

void Jettison(vector<MODULE_CACHE>::iterator it, uint dockedClientID, uint carrierClientID)
{
	bool isInMenu = HkIsInCharSelectMenu(dockedClientID);

	if (dockedClientID != -1 && !isInMenu)
	{
		PrintUserCmdText(carrierClientID, L"Jettisoning " + it->occupiedBy + L"...");
		Watcher.ReleaseModule(carrierClientID, it->occupiedBy);
		JettisoningClients[dockedClientID] = true;

		if (Players[carrierClientID].iBaseID)
		{
			ForceLandingClients[dockedClientID] = Players[carrierClientID].iBaseID;
			POBForceLandingClients[dockedClientID] = Clients[carrierClientID].POBID;
			PrintUserCmdText(dockedClientID, L" ChangeSys %u", Players[dockedClientID].iSystemID);
		}
		else
		{
			Vector pos;
			Matrix ornt;
			pub::SpaceObj::GetLocation(Players[carrierClientID].iShipID, pos, ornt);

			uint undockDistance = Watcher.moduleArchInfo[it->archID].undockDistance;
			pos.x += ornt.data[0][1] * undockDistance;
			pos.y += ornt.data[1][1] * undockDistance;
			pos.z += ornt.data[2][1] * undockDistance;

			mapDeferredJumps[dockedClientID].system = Players[carrierClientID].iSystemID;
			mapDeferredJumps[dockedClientID].pos = pos;
			mapDeferredJumps[dockedClientID].ornt = ornt;

			PrintUserCmdText(dockedClientID, L" ChangeSys %u", Players[carrierClientID].iSystemID);
		}
	}
	else
	{
		if (!Players[carrierClientID].iBaseID)
		{
			PrintUserCmdText(carrierClientID, L"Cannot jettison " + it->occupiedBy + L" while being in space. Dock to a base.");
		}
		else
		{
			// Kick the client, can't update data correctly while client is in character select menu.
			if (isInMenu)
				HkKick(ARG_CLIENTID(dockedClientID));

			Watcher.ReleaseModule(carrierClientID, it->occupiedBy);
			PrintUserCmdText(carrierClientID, L"Jettisoning " + it->occupiedBy + L"...");
			pub::Audio::PlaySoundEffect(carrierClientID, ID_sound_undocked);

			OfflineData Data = Clients[it->occupiedBy];
			Data.Location.baseID = Players[carrierClientID].iBaseID;
			Data.POBID = Clients[carrierClientID].POBID;
			Data.Location.systemID = Players[carrierClientID].iSystemID;
			Data.DockedToModule = 0;
			Data.Save();
		}
	}
}

void SwitchSystem(uint iClientID, uint iShip)
{	
	// Patch the system switch out routine to put the ship in a system of our choosing.

	PBYTE SwitchOut = (PBYTE)hModServer + 0xf600;

	DWORD dummy;
	VirtualProtect(SwitchOut + 0xd7, 200, PAGE_EXECUTE_READWRITE, &dummy);

	SwitchOut[0x0d7] = 0xeb;				// ignore exit object
	SwitchOut[0x0d8] = 0x40;
	SwitchOut[0x119] = 0xbb;				// set the destination system
	*(PDWORD)(SwitchOut + 0x11a) = mapDeferredJumps[iClientID].system;
	SwitchOut[0x266] = 0x45;				// don't generate warning
	*(float*)(SwitchOut + 0x2b0) = mapDeferredJumps[iClientID].pos.z;		// set entry location
	*(float*)(SwitchOut + 0x2b8) = mapDeferredJumps[iClientID].pos.y;
	*(float*)(SwitchOut + 0x2c0) = mapDeferredJumps[iClientID].pos.x;
	*(float*)(SwitchOut + 0x2c8) = mapDeferredJumps[iClientID].ornt.data[2][2];
	*(float*)(SwitchOut + 0x2d0) = mapDeferredJumps[iClientID].ornt.data[1][1];
	*(float*)(SwitchOut + 0x2d8) = mapDeferredJumps[iClientID].ornt.data[0][0];
	*(float*)(SwitchOut + 0x2e0) = mapDeferredJumps[iClientID].ornt.data[2][1];
	*(float*)(SwitchOut + 0x2e8) = mapDeferredJumps[iClientID].ornt.data[2][0];
	*(float*)(SwitchOut + 0x2f0) = mapDeferredJumps[iClientID].ornt.data[1][2];
	*(float*)(SwitchOut + 0x2f8) = mapDeferredJumps[iClientID].ornt.data[1][0];
	*(float*)(SwitchOut + 0x300) = mapDeferredJumps[iClientID].ornt.data[0][2];
	*(float*)(SwitchOut + 0x308) = mapDeferredJumps[iClientID].ornt.data[0][1];
	*(PDWORD)(SwitchOut + 0x388) = 0x03ebc031;		// ignore entry object

	mapDeferredJumps.erase(iClientID);
	pub::SpaceObj::SetInvincible(iShip, false, false, 0);
	Server.SystemSwitchOutComplete(iShip, iClientID);

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

// Checks if carrier ship has all to resupply docked ship and returns time required for it. Notifies only about possibility if notify = true, or notifies only about impossibility if notify = false.
int CheckIfResupplyingAvailable(uint carrierClientID, uint dockedClientID, uint moduleArch, SUPPLIES_INFO& info, bool notify)
{
	MODULE_ARCH &module = Watcher.moduleArchInfo[moduleArch];
	int basicResypplyTime = module.basicResupplyTime;

	if (basicResypplyTime == -1)
	{
		if (!notify)
			PrintUserCmdText(dockedClientID, L"No resupplying possible.");
		return -1;
	}

	ID_TRAITS &traits = Watcher.Cache[carrierClientID].dockingTraits;
	uint crew = 0;

	char *szClassPtr;
	memcpy(&szClassPtr, &Players, 4);
	szClassPtr += 0x418 * (carrierClientID - 1);

	EQ_ITEM *eqLst;
	memcpy(&eqLst, szClassPtr + 0x27C, 4);
	EQ_ITEM *item;
	item = eqLst->next;
	while (item != eqLst)
	{
		// Check if this is crew item.
		if (find(traits.suitableCrewIDs.begin(), traits.suitableCrewIDs.end(), item->iArchID) != traits.suitableCrewIDs.end())
		{
			crew += item->iCount;
			item = item->next;
			continue;
		}

		// Check if this is supply item.
		auto supply = traits.supplyItems.find(item->iArchID);
		if (supply != traits.supplyItems.end())
		{
			if (supply->second.ammoPerUnit)
			{
				info.hasAmmoSup = true;
			}
			if (supply->second.botsPerUnit)
			{
				info.hasBotsSup = true;
			}
			if (supply->second.batsPerUnit)
			{
				info.hasShieldSup = true;
			}
			if (supply->second.cloakBatsPerUnit)
			{
				info.hasCloakSup = true;
			}
		}

		item = item->next;
	}

	uint crewLimit = module.minCrewLimit * traits.crewLimitMultiplier;

	if (crew < crewLimit)
	{
		if (!notify)
			PrintUserCmdText(dockedClientID, L"Carrier ship has not enough crew.");
		return -1;
	}
	else if (!info.hasAmmoSup && !info.hasCloakSup  && !info.hasShieldSup && !info.hasBotsSup)
	{
		if (!notify)
			PrintUserCmdText(dockedClientID, L"Carrier has no supplies.");
		return -1;
	}
	else if (notify)
		PrintUserCmdText(dockedClientID, L"Resupplying system available. Insert '/loadsupplies' to use.");

	if (traits.crewLimitMultiplier)
		return (int)(basicResypplyTime / pow(2, ((crew - crewLimit) / 200.0f)));
	else
		return (int)(basicResypplyTime / 2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Command handlers
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Commands
{
	bool Listdocked(uint client, const wstring& wscCmd)
	{
		PrintUserCmdText(client, EnumerateDockedShips(client));
		return true;
	}

	bool Conn(uint client, const wstring& wscCmd)
	{
		// This plugin always runs before the Conn Plugin runs it's /conn function. Verify that there are no docked ships.
		if (!Clients[client].DockedChars.Empty())
		{
			PrintUserCmdText(client, L"You cannot use this command if you have vessels docked with you!");
			return true;
		}

		// Verify that client is not currently docked to carrier.
		if (Clients[client].DockedToModule)
		{
			PrintUserCmdText(client, L"Connecticut virtual reality module is not found on that base.");
			return true;
		}

		return false;
	}

	bool Return(uint client, const wstring& wscCmd)
	{
		// This plugin always runs before the Conn Plugin runs it's /return function. Verify that there are no docked ships.
		if (!Clients[client].DockedChars.Empty())
		{
			PrintUserCmdText(client, L"You cannot use this command if you have vessels docked with you!");
			return true;
		}

		return false;
	}

	bool Renameme(uint client, const wstring& wscCmd)
	{
		// Don't want to copy whole command here. Just restricted.
		if (Clients[client].DockedToModule)
		{
			PrintUserCmdText(client, L"You cannot use this command if you are docked to someone!");
			return true;
		}

		if (!Clients[client].DockedChars.Empty())
		{
			PrintUserCmdText(client, L"You cannot use this command if you have vessels docked to you!");
			return true;
		}

		return false;
	}

	bool Allowdock(uint iClientID, const wstring& wscCmd)
	{
		// Check if it is base-docking.
		wstring targetName = Trim(GetParam(wscCmd, ' ', 1));
		if (!targetName.empty() && Players[iClientID].iBaseID)
		{
			uint iTargetClientID = HkGetClientIdFromCharname(targetName);
			if (mapPendingDockingRequests[iTargetClientID] == iClientID)
			{
				vector<MODULE_CACHE> &Modules = Watcher.Cache[iClientID].Modules;
				ErrorMessage msg = TryDockAtBase(Modules, iTargetClientID, iClientID, targetName);

				switch (msg)
				{
				case NO_MODULES:		PrintUserCmdText(iClientID, L"You have no docking modules mounted."); break;
				case TOO_LARGE:			PrintUserCmdText(iClientID, L"The ship is too large to dock."); break;
				case NO_FREE_MODULES:	PrintUserCmdText(iClientID, L"You have no free docking modules."); break;
				case OK:				break;
				}

				return true;
			}
		}

		// If charname argument is empty and we're not in space, then ignore the request.
		uint iShip;
		pub::Player::GetShip(iClientID, iShip);
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

		// Find the docking request. If none - ignore.
		if (mapPendingDockingRequests[iTargetClientID] != iClientID)
		{
			PrintUserCmdText(iClientID, L"No pending docking requests for this ship");
			return true;
		}

		vector<MODULE_CACHE> &Modules = Watcher.Cache[iClientID].Modules;
		targetName = (const wchar_t*)Players.GetActiveCharacterName(iTargetClientID);
		ErrorMessage msg = TryDockInSpace(Modules, iTargetClientID, iClientID, iTargetShip, iShip, targetName);

		switch (msg)
		{
		case TOO_LARGE:			PrintUserCmdText(iClientID, L"The ship is too large to dock."); break;
		case NO_FREE_MODULES:	PrintUserCmdText(iClientID, L"You have no free docking modules."); break;
		case TOO_FAR:			PrintUserCmdText(iClientID, L"The ship is too far from yours."); break;
		case OK:				PrintUserCmdText(iClientID, L"You allowed " + targetName + L" to dock."); break;
		}

		return true;
	}

	bool Dockatbase(uint iClientID, const wstring& wscCmd)
	{
		if (mapDockingClients[iClientID])
			return true;

		wstring targetName = GetParam(wscCmd, ' ', 1);
		uint iTargetClientID = HkGetClientIdFromCharname(targetName);

		if (targetName.empty())
		{
			PrintUserCmdText(iClientID, L"Usage: /dockatbase <charname>");
			return true;
		}
		else if (Players[iClientID].iBaseID == 0)
		{
			PrintUserCmdText(iClientID, L"You cannot use this command in space!");
			return true;
		}
		else if (iTargetClientID == -1)
		{
			PrintUserCmdText(iClientID, L"Character was not found at server.");
			return true;
		}
		else if (iTargetClientID == iClientID)
		{
			PrintUserCmdText(iClientID, L"You cannot use this command on yourself!");
			return true;
		}
		else if (mapPendingDockingRequests[iClientID] == iTargetClientID)
		{
			PrintUserCmdText(iClientID, L"You have already sent request to the player.");
			return true;
		}
		else
		{
			uint dockingPOB = Clients[iClientID].POBID;
			uint carrierPOB = Clients[iTargetClientID].POBID;
			bool inSamePOBs = ((!dockingPOB && !carrierPOB) || (dockingPOB == carrierPOB));
			bool inSameBases = (Players[iTargetClientID].iBaseID == Players[iClientID].iBaseID);

			if (!inSameBases || !inSamePOBs)
			{
				PrintUserCmdText(iClientID, L"You must be in same base as carrier.");
				return true;
			}

			vector<MODULE_CACHE> &Modules = Watcher.Cache[iTargetClientID].Modules;
			wstring charname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
			ErrorMessage msg = TryDockAtBase(Modules, iClientID, iTargetClientID, charname);

			switch (msg)
			{
			case NO_MODULES:		PrintUserCmdText(iClientID, L"The ship has no docking modules."); break;
			case TOO_LARGE:			PrintUserCmdText(iClientID, L"Your ship is too large to dock."); break;
			case NO_FREE_MODULES:	PrintUserCmdText(iClientID, L"The carrier has no free docking modules for you."); break;
			case OK:				break;
			}
		}

		return true;
	}

	bool Jettisonship(uint iClientID, const wstring& wscCmd)
	{
		wstring targetName = GetParam(wscCmd, ' ', 1);
		uint iTargetClientID = HkGetClientIdFromCharname(targetName);

		if (targetName.empty())
		{
			PrintUserCmdText(iClientID, L"Usage: /jettisonship <charname>");
			return true;
		}
		else
		{
			vector<MODULE_CACHE> DockedChars = Clients[iClientID].DockedChars.Get();
			for (vector<MODULE_CACHE>::iterator it = DockedChars.begin(); it != DockedChars.end(); it++)
			{
				if (it->occupiedBy == targetName)
				{
					Jettison(it, iTargetClientID, iClientID);
					return true;
				}
			}
		}

		PrintUserCmdText(iClientID, targetName + L" is not docked to you.");
		return true;
	}

	bool Jettisonallships(uint iClientID, const wstring& wscCmd)
	{
		vector<MODULE_CACHE> DockedChars = Clients[iClientID].DockedChars.Get();
		for (vector<MODULE_CACHE>::iterator it = DockedChars.begin(); it != DockedChars.end(); it++)
			Jettison(it, HkGetClientIdFromCharname(it->occupiedBy), iClientID);

		return true;
	}

	bool Loadsupplies(uint iClientID, const wstring& wscCmd)
	{
		// If ship is not docked to player - ignore.
		if (!Clients[iClientID].DockedToModule)
			return true;

		wstring carrierCharname = Clients[iClientID].DockedWith;
		uint carrierClientID = HkGetClientIdFromCharname(carrierCharname);

		// If carrier ship is at server - process request, otherwise send message that it is not possible.
		if (carrierClientID == -1)
		{
			PrintUserCmdText(iClientID, L"Carrier pilot went to bathroom and does not respond. Resupplying disabled.");
			return true;
		}

		wstring dockedCharname = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID));

		// If client is already resupplying - cancel operation.
		if (ResupplyingClients[iClientID])
		{
			ResupplyingClients[iClientID] = 0;
			pub::Audio::PlaySoundEffect(iClientID, ID_sound_canceled);
			PrintUserCmdText(iClientID, L"Resupplying canceled.");

			resupplyList.erase(remove_if(resupplyList.begin(), resupplyList.end(),
				[iClientID](const ActionResupply &action) { return action.dockedClientID == iClientID; }));

			return true;
		}

		Archetype::Ship *ship = Archetype::GetShip(Players[iClientID].iShipArchetype);

		RESUPPLY_REQUEST request;
		request.cloak = 0;
		request.nanobotsInCart = ship->iMaxNanobots;
		request.batteriesInCart = ship->iMaxShieldBats;
		request.cloakBatteriesInCart = 0;

		map<uint, int> ammo;
		map<uint, int> guns;

		// Check what player already has.
		char *szClassPtr;
		memcpy(&szClassPtr, &Players, 4);
		szClassPtr += 0x418 * (iClientID - 1);

		EQ_ITEM *eqLst;
		memcpy(&eqLst, szClassPtr + 0x27C, 4);
		EQ_ITEM *item;
		item = eqLst->next;
		while (item != eqLst)
		{
			// Check if item present in plugin gun database.
			if (item->bMounted)
			{
				if (mapAmmo.find(item->iArchID) != mapAmmo.end())
				{
					if (guns.find(item->iArchID) == guns.end())
						guns[item->iArchID] = 1;
					else if (mapAmmo[item->iArchID].stackable)
						guns[item->iArchID]++;
				}

				// Check if item present in plugin cloaking devices database.
				if (mapBatteries.find(item->iArchID) != mapBatteries.end())
				{
					request.cloak = item->iArchID;
				}
			}
			// Check if item present in plugin ammo database.
			else
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
				// Is this shield batteries?
				if (item->iArchID == ID_batteries)
				{
					request.batteriesInCart = ship->iMaxShieldBats - item->iCount;
				}
				// Is this nanobots?
				if (item->iArchID == ID_nanobots)
				{
					request.nanobotsInCart = ship->iMaxNanobots - item->iCount;
				}
			}

			item = item->next;
		}

		// Calculate how much ammo this player needs.
		for (map<uint, int>::iterator it = guns.begin(); it != guns.end(); it++)
		{
			uint ammoID = mapAmmo[it->first].ammoID;

			int need = mapAmmo[it->first].ammoLimit * it->second;
			if (ammo.find(ammoID) != ammo.end())
				need -= ammo[ammoID];

			if(need)
				request.ammoInCart[ammoID] = need;
		}

		if (request.cloak)
		{
			uint ammoID = mapBatteries[request.cloak];

			int need = 1000;
			if (ammo.find(ammoID) != ammo.end())
				need -= ammo[ammoID];

			request.cloakBatteriesInCart = need;
		}

		if (request.ammoInCart.size() + request.cloakBatteriesInCart + request.batteriesInCart + request.nanobotsInCart == 0)
		{
			PrintUserCmdText(iClientID, L"Resupplying is not needed.");
			return true;
		}

		// Calculate time needed for resupplying. Return if carrier can't provide resupplying and notify docked ship about that.
		SUPPLIES_INFO info = SUPPLIES_INFO();
		int time = CheckIfResupplyingAvailable(carrierClientID, iClientID, Clients[iClientID].DockedToModule, info, false);
		if (time == -1) return true;

		// Check if carrier is lacking specific supplies and warn docked ship about that.
		if (info.hasAmmoSup == false && request.ammoInCart.size() != 0)
		{
			request.ammoInCart.clear();
			if (request.cloakBatteriesInCart + request.batteriesInCart + request.nanobotsInCart != 0)
				PrintUserCmdText(iClientID, L"Warning. Carrier has no ammo for you.");
		}

		if (info.hasCloakSup == false && request.cloakBatteriesInCart != 0)
		{
			request.cloakBatteriesInCart = 0;
			if (request.ammoInCart.size() + request.batteriesInCart + request.nanobotsInCart != 0)
				PrintUserCmdText(iClientID, L"Warning. Carrier has no batteries for your cloaking device.");
		}

		if (info.hasBotsSup == false && request.nanobotsInCart != 0)
		{
			request.nanobotsInCart = 0;
			if (request.ammoInCart.size() + request.cloakBatteriesInCart + request.batteriesInCart != 0)
				PrintUserCmdText(iClientID, L"Warning. Carrier has no nanobots for your ship.");
		}

		if (info.hasShieldSup == false && request.batteriesInCart != 0)
		{
			request.batteriesInCart = 0;
			if (request.ammoInCart.size() + request.cloakBatteriesInCart + request.nanobotsInCart != 0)
				PrintUserCmdText(iClientID, L"Warning. Carrier has no shield batteries for your ship.");
		}

		if (request.ammoInCart.size() + request.cloakBatteriesInCart + request.batteriesInCart + request.nanobotsInCart == 0)
		{
			PrintUserCmdText(iClientID, L"Resupplying is not possible. Carrier has no requested supplies in cargo hold.");
			return true;
		}

		pub::Audio::PlaySoundEffect(iClientID, ID_sound_accepted);
		PrintUserCmdText(iClientID, L"Resupplying begins. Wait %i seconds before work is finished.", time);

		// Start timer.
		DelayedResupply(iClientID, dockedCharname, carrierCharname, request, time);
		return true;
	}
}