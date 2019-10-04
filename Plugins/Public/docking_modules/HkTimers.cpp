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
	for (vector<uint>::iterator lit = dockingList.begin(); lit != dockingList.end(); )
	{
		vector<ActionDocking> &queue = dockingQueues[*lit];
		if (!queue.empty())
		{
			// Process only first ship in queue, others need to wait.
			vector<ActionDocking>::iterator it = queue.begin();
			it->timeLeft--;
			if (it->timeLeft == 0)
			{
				// Check if distance between ships is still within limit.
				if (HkDistance3DByShip(Players[it->dockingClientID].iShipID, Players[*lit].iShipID) <= it->interruptDistance)
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
				}

				queue.erase(it);
			}
		}

		// If queue of the carrier is empty, remove carrier ID from list.
		if (queue.empty())
			lit = dockingList.erase(lit);
		else
			++lit;
	}
}

void DelayedResupply(uint dockedClientID, wstring &dockedCharname, wstring &carrierCharname, RESUPPLY_REQUEST &request, int delayTimeSeconds)
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
	for (vector<ActionResupply>::iterator it = resupplyList.begin(); it != resupplyList.end(); )
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
				continue;
			}

			if (carrierClientID == -1 || HkIsInCharSelectMenu(carrierClientID))
			{
				PrintUserCmdText(dockedClientID, L"Carrier pilot went to bathroom and does not respond. Resupplying disrupted.");
				ResupplyingClients[dockedClientID] = false;
				it = resupplyList.erase(it);
				continue;
			}

			// Unpack request.
			uint &cloak = it->request.cloak;
			map<uint, int> &ammoInCart = it->request.ammoInCart;
			int &cloakBatteriesInCart = it->request.cloakBatteriesInCart;
			int &nanobotsInCart = it->request.nanobotsInCart;
			int &batteriesInCart = it->request.batteriesInCart;

			// Start resupplying.
			ID_TRAITS &traits = Watcher.Cache[carrierClientID].dockingTraits;

			EQ_ITEM *item;
			traverse_equipment(carrierClientID, item)
			{
				auto supplyIter = traits.supplyItems.find(item->iArchID);
				if (supplyIter != traits.supplyItems.end())
				{
					SUPPLY &supply = supplyIter->second;
					if (supply.ammoPerUnit && !it->request.ammoInCart.empty())
					{
						uint &efficiency = supply.ammoPerUnit;
						int toUse = 0;
						int toAdd = item->iCount * efficiency;
						int boost = 1;

						for (map<uint, int>::iterator ait = ammoInCart.begin(); (ait != ammoInCart.end() && toAdd != 0); )
						{
							auto boostIter = mapBoostedAmmo.find(ait->first);
							if (boostIter != mapBoostedAmmo.end())
							{
								boost = boostIter->second;
								toAdd = toAdd * boost;
							}
							else if (boost > 1)
							{
								toAdd = toAdd / boost;
								boost = 1;
							}

							int ammotoAdd;
							if (ait->second <= toAdd)
								ammotoAdd = ait->second;
							else
								ammotoAdd = toAdd;

							ait->second -= ammotoAdd;
							toAdd -= ammotoAdd;
							toUse += ammotoAdd / efficiency / boost;

							HkAddCargo(ARG_CLIENTID(dockedClientID), ait->first, ammotoAdd, false);

							if (ait->second == 0)
								ait = ammoInCart.erase(ait);
							else
								++ait;
						}

						HkRemoveCargo(ARG_CLIENTID(carrierClientID), item->sID, toUse);
					}
					if (supply.batsPerUnit && batteriesInCart != 0)
					{
						uint &efficiency = supply.batsPerUnit;
						int toUse = 0;
						int toAdd = item->iCount * efficiency;

						if (batteriesInCart < toAdd)
							toAdd = batteriesInCart;

						batteriesInCart -= toAdd;
						toUse = (int)ceil(toAdd / efficiency);

						if (toAdd)
							HkAddCargo(ARG_CLIENTID(dockedClientID), ID_item_batteries, toAdd, false);

						HkRemoveCargo(ARG_CLIENTID(carrierClientID), item->sID, toUse);
					}
					if (supply.botsPerUnit && nanobotsInCart != 0)
					{
						uint &efficiency = supply.botsPerUnit;
						int toUse = 0;
						int toAdd = item->iCount * efficiency;

						if (nanobotsInCart < toAdd)
							toAdd = nanobotsInCart;

						toUse = (int)ceil(toAdd / efficiency);
						nanobotsInCart -= toAdd;

						if (toAdd)
							HkAddCargo(ARG_CLIENTID(dockedClientID), ID_item_nanobots, toAdd, false);

						HkRemoveCargo(ARG_CLIENTID(carrierClientID), item->sID, toUse);
					}
					if (supply.cloakBatsPerUnit && cloakBatteriesInCart != 0)
					{
						uint &efficiency = supply.cloakBatsPerUnit;
						int toUse = 0;
						int toAdd = item->iCount * efficiency;

						if (cloakBatteriesInCart < toAdd)
							toAdd = cloakBatteriesInCart;

						cloakBatteriesInCart -= toAdd;
						toUse = (int)ceil(toAdd / efficiency);

						if (toAdd)
							HkAddCargo(ARG_CLIENTID(dockedClientID), mapBatteries[cloak], toAdd, false);

						HkRemoveCargo(ARG_CLIENTID(carrierClientID), item->sID, toUse);
					}
				}

				continue_traverse(item);
			}

			pub::Audio::PlaySoundEffect(dockedClientID, ID_sound_resupply);
			PrintUserCmdText(dockedClientID, L"Resupplying complete.");

			ResupplyingClients[dockedClientID] = false;
			it = resupplyList.erase(it);
		}
		else
			++it;
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