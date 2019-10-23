// AlleyPlugin for FLHookPlugin
// April 2015 by Alley
//
// 
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Includes
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <math.h>
#include <float.h>
#include <FLHook.h>
#include <plugin.h>
#include <list>
#include <set>

#include <PluginUtilities.h>
#include "PlayerRestrictions.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Structures and shit yo
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct scistruct
{
	list<uint> canmount;
	list<uint> nomount;
};

map <uint, wstring> shipclassnames;
map <uint, wstring> itemnames;
map <uint, scistruct> shipclassitems;
map <uint, wstring> owned;

map<uint, uint> mapIDs;

//we store these here as it's more efficient than constantly requested what id the player is flying with.
struct pinfo
{
	uint playerid;
	float maxholdsize;
	uint shipclass;
};

map <uint, pinfo> clientplayerid;

//the struct
struct iddockinfo
{
	int type;
	uint cargo;
	list<uint> systems;
	list<uint> shipclasses;
};

//first uint will be the ID hash
map <uint, iddockinfo> iddock;

map<uint, uint> player_last_base;

void SCI::LoadSettings()
{
	// The path to the configuration file.
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\alley_shipclassitems.cfg";
	string idfile = "..\\data\\equipment\\misc_equip.ini";

	INI_Reader ini;
	if (ini.open(scPluginCfgFile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("shipclasses"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("class"))
					{
						shipclassnames[ini.get_value_int(0)] = stows(ini.get_value_string(1));
					}
				}
			}
			else if (ini.is_header("itemnames"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("item"))
					{
						itemnames[CreateID(ini.get_value_string(0))] = stows(ini.get_value_string(1));
					}
				}
			}
			else if (ini.is_header("itemrestrict"))
			{
				scistruct sci;
				uint itemarchid;
				while (ini.read_value())
				{
					if (ini.is_value("item"))
					{
						itemarchid = CreateID(ini.get_value_string(0));
					}
					else if (ini.is_value("canmount"))
					{
						sci.canmount.push_back(ini.get_value_int(0));
					}
					else if (ini.is_value("nomount"))
					{
						sci.nomount.push_back(CreateID(ini.get_value_string(0)));
					}
				}
				shipclassitems[itemarchid] = sci;
			}
			else if (ini.is_header("idrestrict"))
			{
				iddockinfo info;
				uint id;
				while (ini.read_value())
				{
					if (ini.is_value("name"))
					{
						id = CreateID(ini.get_value_string(0));
					}
					else if (ini.is_value("type"))
					{
						info.type = ini.get_value_int(0);
					}
					else if (ini.is_value("cargo"))
					{
						info.cargo = ini.get_value_int(0);
					}
					else if (ini.is_value("shipclass"))
					{
						info.shipclasses.push_back(ini.get_value_int(0));
					}
					else if (ini.is_value("system"))
					{
						info.systems.push_back(CreateID(ini.get_value_string(0)));
					}
				}
				iddock[id] = info;
			}
		}
		ini.close();
	}
	if (ini.open(idfile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("Tractor"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("nickname"))
					{
						mapIDs[CreateID(ini.get_value_string(0))] = 0;
					}
				}
			}
		}
		ini.close();
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Dependencies
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SCI::CheckItems(unsigned int iClientID)
{
	bool foundamountedid = false;
	// Check all items on ship, see if one isn't meant for this ship class.
	for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
	{
		//we stick in a small check to see if there's an ID mounted
		if (mapIDs.find(item->iArchID) != mapIDs.end())
		{
			if (item->bMounted)
			{
				foundamountedid = true;
			}
		}

		if (shipclassitems.find(item->iArchID) != shipclassitems.end())
		{
			if (item->bMounted)
			{
				//more efficent to find out if the item we want is mounted first, then we get the data for the error message if necessary.
				for (map<uint, scistruct>::iterator iter = shipclassitems.begin(); iter != shipclassitems.end(); iter++)
				{
					if (iter->first == item->iArchID)
					{
						Archetype::Ship* TheShipArch = Archetype::GetShip(Players[iClientID].iShipArchetype);
						wstring classname = shipclassnames.find(TheShipArch->iShipClass)->second;
						//PrintUserCmdText(iClientID, L"DEBUG: This ship class is known as %s", classname.c_str());
						bool foundclass = false;

						// find if the ship class match
						list<uint>::iterator iterclass = iter->second.canmount.begin();
						while (iterclass != iter->second.canmount.end())
						{
							if (*iterclass == TheShipArch->iShipClass)
							{
								foundclass = true;
								//PrintUserCmdText(iClientID, L"DEBUG: This ship class is OK for item %s.", itemnames.find(iter->first)->second.c_str());
								break;
							}
							iterclass++;
						}

						if (foundclass == false)
						{
							//PrintUserCmdText(iClientID, L"DEBUG: Tagged for ownage");
							wstring wscMsg = L"ERR you can't undock with %item mounted. This item can't be mounted on a %shipclass.";
							wscMsg = ReplaceStr(wscMsg, L"%item", itemnames.find(iter->first)->second.c_str());
							wscMsg = ReplaceStr(wscMsg, L"%shipclass", classname.c_str());
							owned[iClientID] = wscMsg;
							StoreReturnPointForClient(iClientID);
							return;
						}
						// check for non-stackable items
						else
						{
							for (list<EquipDesc>::iterator itemstack = Players[iClientID].equipDescList.equip.begin(); itemstack != Players[iClientID].equipDescList.equip.end(); itemstack++)
							{
								if (itemstack->bMounted)
								{
									bool founditernostack = false;
									// find if a mounted item match the non-stack list
									list<uint>::iterator iternostack = iter->second.nomount.begin();
									while (iternostack != iter->second.nomount.end())
									{
										if (*iternostack == itemstack->iArchID)
										{
											founditernostack = true;
											//PrintUserCmdText(iClientID, L"DEBUG: Found non-stackable item %s.", itemnames.find(itemstack->equip.iArchID)->second.c_str());
											break;
										}
										iternostack++;
									}

									if (founditernostack == true)
									{
										wstring wscMsg = L"ERR You are not allowed to have %item and %second mounted at the same time.";
										wscMsg = ReplaceStr(wscMsg, L"%item", itemnames.find(iter->first)->second.c_str());
										wscMsg = ReplaceStr(wscMsg, L"%second", itemnames.find(itemstack->iArchID)->second.c_str());
										owned[iClientID] = wscMsg;
										StoreReturnPointForClient(iClientID);
										return;
									}
								}
							}
						}
						//PrintUserCmdText(iClientID, wscMsg);
						break;
					}
				}
			}
		}
	}

	if (foundamountedid == false)
	{
		wstring wscMsg = L"ERR You have no ID on your ship. You must have one.";
		owned[iClientID] = wscMsg;
		StoreReturnPointForClient(iClientID);
	}


	return;
}

// based on conn plugin
void SCI::StoreReturnPointForClient(unsigned int client)
{
	// Use *LAST* player base rather than current one because base's BaseExit handler
	// could have run before us and killed the record of being docked on a POB before
	// we got to run CheckItems and StoreReturnPointForClient
	uint base = GetCustomLastBaseForClient(client);
	// It's not docked at a custom base, check for a regular base
	if (!base) {
		pub::Player::GetBase(client, base);
	}
	if (!base) {
		return;
	}

	player_last_base[client] = base;
}

// based on conn plugin
uint SCI::GetCustomLastBaseForClient(unsigned int client)
{
	// Pass to plugins incase this ship is docked at a custom base.
	CUSTOM_BASE_LAST_DOCKED_STRUCT info;
	info.iClientID = client;
	info.iLastDockedBaseID = 0;
	Plugin_Communication(CUSTOM_BASE_LAST_DOCKED, &info);
	return info.iLastDockedBaseID;
}

// based on conn plugin
void SCI::MoveClient(unsigned int client, unsigned int targetBase)
{
	// Ask that another plugin handle the beam.
	CUSTOM_BASE_BEAM_STRUCT info;
	info.iClientID = client;
	info.iTargetBaseID = targetBase;
	info.bBeamed = false;
	Plugin_Communication(CUSTOM_BASE_BEAM, &info);
	if (info.bBeamed)
		return;

	// No plugin handled it, do it ourselves.
	pub::Player::ForceLand(client, targetBase); // beam
}

void SCI::CheckOwned(unsigned int iClientID)
{
	//end = not found
	if (owned.find(iClientID) == owned.end())
	{
		//PrintUserCmdText(iClientID, L"DEBUG: Not found for ownage.");
	}
	//else = found
	else
	{
		PrintUserCmdText(iClientID, owned.find(iClientID)->second.c_str());
		MoveClient(iClientID, player_last_base[iClientID]);
		//PrintUserCmdText(iClientID, L"DEBUG: Owned. Sending you back to base.");
		owned.erase(iClientID);
		player_last_base.erase(iClientID);
	}
}

void SCI::ClearClientInfo(uint iClientID)
{
	owned.erase(iClientID);
	clientplayerid.erase(iClientID);
}

void SCI::UpdatePlayerID(unsigned int iClientID)
{
	// Retrieve the location and cargo list.
	int iHoldSize;
	list<CARGO_INFO> lstCargo;
	HkEnumCargo((const wchar_t*)Players.GetActiveCharacterName(iClientID), lstCargo, iHoldSize);

	foreach(lstCargo, CARGO_INFO, i)
	{
		if (i->bMounted)
		{
			// is it a good id
			for (map<uint, iddockinfo>::iterator iter = iddock.begin(); iter != iddock.end(); iter++)
			{
				if (iter->first == i->iArchID)
				{
					pinfo info;
					//PrintUserCmdText(iClientID, L"DEBUG: zonerzonerzonerzoner");
					info.playerid = i->iArchID;

					Archetype::Ship *ship = Archetype::GetShip(Players[iClientID].iShipArchetype);
					info.maxholdsize = ship->fHoldSize;
					info.shipclass = ship->iShipClass;

					clientplayerid[iClientID] = info;

					return;
				}
			}
		}
	}

	//PrintUserCmdText(iClientID, L"DEBUG: Cat = no");
	return;
}

bool SCI::CanDock(uint iDockTarget, uint iClientID)
{
	//First we check if the player is on our watchlist.
	if (clientplayerid.find(iClientID) != clientplayerid.end())
	{
		//PrintUserCmdText(iClientID, L"DEBUG: Found you");
		//temporarily copy the id so we don't mapception
		uint id = clientplayerid[iClientID].playerid;
		uint currsystem = Players[iClientID].iSystemID;
		bool arewe = false;

		//Are we in a system we care about
		for (list<uint>::iterator iter = iddock[id].systems.begin(); iter != iddock[id].systems.end(); iter++)
		{
			if (*iter == currsystem)
			{
				arewe = true;
				//PrintUserCmdText(iClientID, L"DEBUG: We are in a system we care about");
				break;
			}
		}

		if (arewe == true)
		{
			uint iTypeID;
			pub::SpaceObj::GetType(iDockTarget, iTypeID);

			if (iTypeID == OBJ_DOCKING_RING || iTypeID == OBJ_STATION)
			{
				//we check the cargo restriction first as that should iron out a good chunk of them
				if (clientplayerid[iClientID].maxholdsize > iddock[id].cargo)
				{
					PrintUserCmdText(iClientID, L"Cargo Hold is over the authorized capacity. Docking Denied.");
					return false;
				}

				uint currshipclass = clientplayerid[iClientID].shipclass;
				for (list<uint>::iterator iter = iddock[id].shipclasses.begin(); iter != iddock[id].shipclasses.end(); iter++)
				{
					if (*iter == currshipclass)
					{
						PrintUserCmdText(iClientID, L"This ship class is not allowed to dock in this system. Docking Denied.");
						return false;
					}
				}
			}
			else
			{
				return true;
			}
		}

		//PrintUserCmdText(iClientID, L"DEBUG: We are not in a system we care about");
	}

	return true;
}
