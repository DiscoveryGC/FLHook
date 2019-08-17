#include "Main.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Lists of delayed actions handled by timers.

vector<uint> dockingList;
vector<ActionResupply> resupplyList;

void DelayedDocking(uint dockingClientID, uint carrierClientID, uint moduleArch, uint interruptDistance, int delayTimeSeconds)
{
	ActionDocking action;
	action.timeLeft = delayTimeSeconds;
	action.interruptDistance = interruptDistance;
	action.dockingClientID = dockingClientID;
	action.moduleArch = moduleArch;

	mapDockingClients[dockingClientID] = carrierClientID;

	if (dockingQueues[carrierClientID].empty())
		dockingList.push_back(carrierClientID);

	dockingQueues[carrierClientID].push_back(action);
}

void DockingTimer()
{
	for (vector<uint>::iterator lit = dockingList.begin(); lit != dockingList.end(); lit++)
	{
		if (!dockingQueues[*lit].empty())
		{
			// Process only first ship in queue, others need to wait.
			vector<ActionDocking>::iterator it = dockingQueues[*lit].begin();
			it->timeLeft--;
			if (it->timeLeft == 0)
			{
				// Check if distance between ships is still within limit.
				if (HkDistance3DByShip(Players[it->dockingClientID].iShipID, Players[*lit].iShipID) > it->interruptDistance)
				{
					dockingQueues[*lit].erase(it);
				}
				else
				{
					OnlineData Data = Clients[it->dockingClientID];
					Data.saveLastBaseID = Players[it->dockingClientID].iLastBaseID;
					Data.saveLastPOBID = Clients[it->dockingClientID].POBID;
					Data.DockedWith = (wstring)(const wchar_t*)Players.GetActiveCharacterName(*lit);
					Data.DockedToModule = it->moduleArch;

					uint iBaseID = GetProxyBaseForCarrier(*lit);
					pub::Player::ForceLand(it->dockingClientID, iBaseID);
					mapDockingClients[it->dockingClientID] = 0;
					pub::Audio::PlaySoundEffect(*lit, ID_sound_docked);
					MiscCmds::ExportSetLights(it->dockingClientID);
					Watcher.OccupyModule(*lit, it->moduleArch, (wstring)(const wchar_t*)Players.GetActiveCharacterName(it->dockingClientID));

					dockingQueues[*lit].erase(it);
				}
			}
		}

		// If queue of the carrier is empty, remove carrier ID from list.
		if (dockingQueues[*lit].empty())
		{
			lit = dockingList.erase(lit);
			lit--;
		}
	}
}

void DelayedResupply(uint dockedClientID, wstring &dockedCharname, wstring &carrierCharname, RESUPPLY_REQUEST request, int delayTimeSeconds)
{
	ActionResupply action;
	action.timeLeft = delayTimeSeconds;
	action.dockedClientID = dockedClientID;
	action.carrierCharname = carrierCharname;
	action.dockedCharname = dockedCharname;
	action.request = request;

	ResupplyingClients[dockedClientID] = true;

	resupplyList.push_back(action);
}

void ResupplyTimer()
{
	for (vector<ActionResupply>::iterator it = resupplyList.begin(); it != resupplyList.end(); it++)
	{
		it->timeLeft--;
		if (it->timeLeft == 0)
		{
			// Check if both are still at server.
			uint carrierClientID = HkGetClientIdFromCharname(it->carrierCharname);
			uint dockedClientID = HkGetClientIdFromCharname(it->dockedCharname);

			if (dockedClientID == -1 || HkIsInCharSelectMenu(dockedClientID))
			{
				it = resupplyList.erase(it);
				it--;
				continue;
			}

			if (carrierClientID == -1 || HkIsInCharSelectMenu(carrierClientID))
			{
				PrintUserCmdText(dockedClientID, L"Carrier pilot went to bathroom and does not respond. Resupplying disrupted.");
				ResupplyingClients[dockedClientID] = false;
				it = resupplyList.erase(it);
				it--;
				continue;
			}

			// Unpack request.
			uint& iCloak = it->request.cloak;
			map<uint, int>& iAmmoInCart = it->request.ammoInCart;
			int& iCloakBatteriesInCart = it->request.cloakBatteriesInCart;
			int& iNanobotsInCart = it->request.nanobotsInCart;
			int& iBatteriesInCart = it->request.batteriesInCart;

			// Start resupplying.
			ID_TRAITS &traits = Watcher.Cache[carrierClientID].dockingTraits;

			char *szClassPtr;
			memcpy(&szClassPtr, &Players, 4);
			szClassPtr += 0x418 * (carrierClientID - 1);

			EQ_ITEM *eqLst;
			memcpy(&eqLst, szClassPtr + 0x27C, 4);
			EQ_ITEM *item;
			item = eqLst->next;
			while (item != eqLst)
			{
				auto supply = traits.supplyItems.find(item->iArchID);
				if (supply != traits.supplyItems.end())
				{
					if (supply->second.ammoPerUnit && it->request.ammoInCart.size() != 0)
					{
						ushort efficiency = supply->second.ammoPerUnit;
						int toUse = 0;
						int toAdd = item->iCount * efficiency;
						int boost = 1;

						for (map<uint, int>::iterator it = iAmmoInCart.begin(); it != iAmmoInCart.end(); it++)
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

							if (toAdd)
								HkAddCargo(ARG_CLIENTID(dockedClientID), it->first, ammotoAdd, false);
						}

						HkRemoveCargo(ARG_CLIENTID(carrierClientID), item->sID, toUse);
					}
					if (supply->second.batsPerUnit && iBatteriesInCart != 0)
					{
						ushort efficiency = supply->second.batsPerUnit;
						int toUse = 0;
						int toAdd = item->iCount * efficiency;

						if (iBatteriesInCart < toAdd)
							toAdd = iBatteriesInCart;

						iBatteriesInCart -= toAdd;
						toUse = (int)ceil(toAdd / efficiency);

						if (toAdd)
							HkAddCargo(ARG_CLIENTID(dockedClientID), ID_batteries, toAdd, false);

						HkRemoveCargo(ARG_CLIENTID(carrierClientID), item->sID, toUse);
					}
					if (supply->second.botsPerUnit && iNanobotsInCart != 0)
					{
						ushort efficiency = supply->second.botsPerUnit;
						int toUse = 0;
						int toAdd = item->iCount * efficiency;

						if (iNanobotsInCart < toAdd)
							toAdd = iNanobotsInCart;

						toUse = (int)ceil(toAdd / efficiency);
						iNanobotsInCart -= toAdd;

						if (toAdd)
							HkAddCargo(ARG_CLIENTID(dockedClientID), ID_nanobots, toAdd, false);

						HkRemoveCargo(ARG_CLIENTID(carrierClientID), item->sID, toUse);
					}
					if (supply->second.cloakBatsPerUnit && iCloakBatteriesInCart != 0)
					{
						ushort efficiency = supply->second.cloakBatsPerUnit;
						int toUse = 0;
						int toAdd = item->iCount * efficiency;

						if (iCloakBatteriesInCart < toAdd)
							toAdd = iCloakBatteriesInCart;

						iCloakBatteriesInCart -= toAdd;
						toUse = (int)ceil(toAdd / efficiency);

						if(toAdd)
							HkAddCargo(ARG_CLIENTID(dockedClientID), mapBatteries[iCloak], toAdd, false);

						HkRemoveCargo(ARG_CLIENTID(carrierClientID), item->sID, toUse);
					}
				}

				item = item->next;
			}

			pub::Audio::PlaySoundEffect(dockedClientID, ID_sound_resupply);
			PrintUserCmdText(dockedClientID, L"Resupplying complete.");

			ResupplyingClients[dockedClientID] = false;
			it = resupplyList.erase(it);
			it--;
		}
	}
}

namespace Timers
{
	void HkTimerCheckKick()
	{
		returncode = DEFAULT_RETURNCODE;

		// Execute these timers every second.
		DockingTimer();
		ResupplyTimer();
	}
}