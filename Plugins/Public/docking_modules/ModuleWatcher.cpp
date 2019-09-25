#include "Main.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Helper functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Get list of docked ships in string format.
wstring EnumerateDockedShips(uint carrierClientID)
{
	vector<MODULE_CACHE> DockedChars = Clients[carrierClientID].DockedChars.Get();

	if (!DockedChars.empty())
	{
		wstring shipNames;

		for (int i = 0; i != DockedChars.size(); ++i)
		{
			if (i != 0)
				shipNames += L", ";

			shipNames += DockedChars[i].occupiedBy;
		}

		return L"Detected ships on board: " + shipNames + L".";
	}
	else if (!Clients[carrierClientID].HasDockingModules)
	{
		return L"No docking modules detected.";
	}

	return L"No docked ships detected on board.";
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Hooked functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace ModuleWatcher
{
	void __stdcall CharacterSelect_AFTER(struct CHARACTER_ID const &cId, uint iClientID)
	{
		// Don't do this all at ClearClientInfo because it's not being fired when player switches characters.
		returncode = DEFAULT_RETURNCODE;

		ForceLandingClients[iClientID] = 0;
		POBForceLandingClients[iClientID] = 0;
		ShipsToLaunch[iClientID] = 0;
		JettisoningClients[iClientID] = 0;
		JumpingCarriers[iClientID] = false;
		JumpingDockedShips[iClientID] = false;
		ResupplyingClients[iClientID] = false;

		bool licenseFound = false;
		vector<MODULE_CACHE> newModules;

		for (auto &item : Players[iClientID].equipDescList.equip)
		{
			if (!item.bMounted)
				continue;

			if (Watcher.moduleArchInfo.find(item.iArchID) != Watcher.moduleArchInfo.end())
				newModules.push_back(MODULE_CACHE(item.iArchID));
			else if (Watcher.IDTraits.find(item.iArchID) != Watcher.IDTraits.end())
			{
				licenseFound = true;
				Watcher.Cache[iClientID].dockingTraits = Watcher.IDTraits[item.iArchID];
			}
		}

		if (!licenseFound)
			Watcher.Cache[iClientID].dockingTraits = defaultTraits;

		vector<MODULE_CACHE> dockedChars = Clients[iClientID].DockedChars.Get();
		for (vector<MODULE_CACHE>::iterator it = newModules.begin(); it != newModules.end(); ++it)
		{
			for (vector<MODULE_CACHE>::const_iterator cit = dockedChars.begin(); cit != dockedChars.end(); ++cit)
			{
				if (it->occupiedBy.empty() && cit->archID == it->archID)
				{
					it->occupiedBy = cit->occupiedBy;
					dockedChars.erase(cit);
					break;
				}
			}
		}

		// Sort modules in capacity ascending order to occupy module rationally.
		// Ship at first tries to occupy smaller modules, later - larger ones.
		sort(newModules.begin(), newModules.end(), [](const MODULE_CACHE &A, const MODULE_CACHE &B)
		{
			return (Watcher.moduleArchInfo[A.archID].maxCargoCapacity < Watcher.moduleArchInfo[B.archID].maxCargoCapacity);
		});

		Watcher.Cache[iClientID].Modules = newModules;
	}

	void __stdcall ReqEquipment_AFTER(class EquipDescList const &edl, uint iClientID)
	{
		returncode = DEFAULT_RETURNCODE;

		bool licenseFound = false;

		// Push all cached modules under threat to be removed from cache.
		vector<MODULE_CACHE> &oldModules = Watcher.Cache[iClientID].Modules;
		vector<MODULE_CACHE> newModules;
		newModules.reserve(oldModules.size());

		// Get relevant docking modules.
		for (auto &item : edl.equip)
		{
			if (!item.bMounted)
				continue;

			if (Watcher.moduleArchInfo.find(item.iArchID) != Watcher.moduleArchInfo.end())
				newModules.push_back(MODULE_CACHE(item.iArchID));
			else if (Watcher.IDTraits.find(item.iArchID) != Watcher.IDTraits.end())
			{
				licenseFound = true;
				Watcher.Cache[iClientID].dockingTraits = Watcher.IDTraits[item.iArchID];
			}
		}

		if (!licenseFound)
			Watcher.Cache[iClientID].dockingTraits = defaultTraits;

		// Move all possible players from old modules to new.
		for (vector<MODULE_CACHE>::iterator oit = oldModules.begin(); oit != oldModules.end(); )
		{
			if (oit->occupiedBy.empty())
			{
				oit = oldModules.erase(oit);
			}
			else
			{
				for (vector<MODULE_CACHE>::iterator nit = newModules.begin(); nit != newModules.end(); ++nit)
				{
					if (oit->archID == nit->archID && nit->occupiedBy.empty())
					{
						nit->occupiedBy = oit->occupiedBy;
						oit = oldModules.erase(oit);
						goto end;
					}
				}

				++oit;
			}
		end:;
		}

		// Eject all remained players.
		for (vector<MODULE_CACHE>::iterator it = oldModules.begin(); it != oldModules.end(); ++it)
		{
			Jettison(it, HkGetClientIdFromCharname(it->occupiedBy), iClientID);
		}

		// Sort modules in capacity ascending order to occupy module rationally.
		// Ship at first tries to occupy smaller modules, later - larger ones.
		sort(newModules.begin(), newModules.end(), [](const MODULE_CACHE &A, const MODULE_CACHE &B)
		{
			return (Watcher.moduleArchInfo[A.archID].maxCargoCapacity < Watcher.moduleArchInfo[B.archID].maxCargoCapacity);
		});

		// Update module list in cache by reference;
		oldModules = newModules;
	}

	void __stdcall ReqAddItem(uint iArchID, char const *cHpName, int iCount, float fHealth, bool bMounted, uint iClientID)
	{
		returncode = DEFAULT_RETURNCODE;

		if (ResupplyingClients[iClientID])
		{
			ResupplyingClients[iClientID] = false;
			pub::Audio::PlaySoundEffect(iClientID, ID_sound_canceled);
			PrintUserCmdText(iClientID, L"ERR Resupply process disrupted.");

			resupplyList.erase(remove_if(resupplyList.begin(), resupplyList.end(),
				[iClientID](const ActionResupply &action) { return action.dockedClientID == iClientID; }));
		}
	}

	void __stdcall ReqAddItem_AFTER(uint iArchID, char const *cHpName, int iCount, float fHealth, bool bMounted, uint iClientID)
	{
		returncode = DEFAULT_RETURNCODE;

		if (!bMounted)
			return;

		if (Watcher.moduleArchInfo.find(iArchID) != Watcher.moduleArchInfo.end())
		{
			vector<MODULE_CACHE> &Modules = Watcher.Cache[iClientID].Modules;
			Modules.push_back(MODULE_CACHE(iArchID));

			// Sort modules in capacity ascending order to occupy module rationally.
			// Ship at first tries to occupy smaller modules, later - larger ones.
			sort(Modules.begin(), Modules.end(), [](const MODULE_CACHE &A, const MODULE_CACHE &B)
			{
				return (Watcher.moduleArchInfo[A.archID].maxCargoCapacity < Watcher.moduleArchInfo[B.archID].maxCargoCapacity);
			});
		}
		else if (Watcher.IDTraits.find(iArchID) != Watcher.IDTraits.end())
		{
			Watcher.Cache[iClientID].dockingTraits = Watcher.IDTraits[iArchID];
		}
	}

	void __stdcall ReqRemoveItem(ushort sHpID, int iCount, uint iClientID)
	{
		returncode = DEFAULT_RETURNCODE;

		vector<MODULE_CACHE> &Modules = Watcher.Cache[iClientID].Modules;

		list<EquipDesc> &equipList = Players[iClientID].equipDescList.equip;
		list<EquipDesc>::iterator item = find_if(equipList.begin(), equipList.end(),
			[sHpID](const EquipDesc &item) { return item.sID == sHpID; });

		if (!item->bMounted)
			return;

		if (Watcher.moduleArchInfo.find(item->iArchID) != Watcher.moduleArchInfo.end())
		{
			// Try to find non-occupied module and remove it.
			for (vector<MODULE_CACHE>::iterator mit = Modules.begin(); mit != Modules.end(); ++mit)
				if (mit->occupiedBy.empty() && mit->archID == item->iArchID)
				{
					Modules.erase(mit);
					return;
				}

			// Well, there are no such modules without players, going to remove module with player.
			for (vector<MODULE_CACHE>::iterator mit = Modules.begin(); mit != Modules.end(); ++mit)
				if (mit->archID == item->iArchID)
				{
					Jettison(mit, HkGetClientIdFromCharname(mit->occupiedBy), iClientID);
					Modules.erase(mit);
					return;
				}
		}
		else if (Watcher.IDTraits.find(item->iArchID) != Watcher.IDTraits.end())
		{
			Watcher.Cache[iClientID].dockingTraits = defaultTraits;
		}
	}

	void __stdcall SPScanCargo_AFTER(uint const &scanningShip, uint const &scannedShip, uint iClientID)
	{
		returncode = DEFAULT_RETURNCODE;

		uint scannedClientID = HkGetClientIDByShip(scannedShip);
		if (scannedClientID && Clients[scannedClientID].HasDockingModules)
			PrintUserCmdText(iClientID, EnumerateDockedShips(scannedClientID));
	}
}