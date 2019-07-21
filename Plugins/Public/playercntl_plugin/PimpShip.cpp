// Player Control plugin for FLHookPlugin
// Feb 2010 by Cannon
//
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

#include <windows.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <math.h>
#include <float.h>
#include <FLHook.h>
#include <plugin.h>
#include <math.h>
#include <list>
#include <set>

#include <PluginUtilities.h>
#include "Main.h"

#include <FLCoreServer.h>
#include <FLCoreCommon.h>


IMPORT unsigned int  MakeLocationID(unsigned int,char const *);

namespace PimpShip
{

	// Intro messages when entering the room.
	static wstring set_wscIntroMsg1 = L"Pimp-my-ship facilities are available here.";
	static wstring set_wscIntroMsg2 = L"Type /pimpship on your console to see options.";

	// Cost per changed item.
	static int set_iCost = 0;

	// List of dealer rooms
	static map<uint, wstring> set_mapDealers;

	// Item of equipment for a single client.
	struct EQ_HARDPOINT
	{
		EQ_HARDPOINT() : sID(0), iArchID(0) {}

		uint sID;
		uint iArchID;
		wstring wscHardPoint;
	};

	// List of connected clients.
	struct INFO
	{
		INFO() : bInPimpDealer(false) {}

		// Map of hard point ID to equip.
		map<uint, EQ_HARDPOINT> mapCurrEquip;

		bool bInPimpDealer;
	};
	static map<uint, INFO> mapInfo;

	// Map of item id to ITEM INFO
	struct ITEM_INFO
	{
		ITEM_INFO() : iArchID(0) {}

		uint iArchID;
		wstring wscNickname;
		wstring wscDescription;
	};
	map<uint, ITEM_INFO> mapAvailableItems;

	bool IsItemArchIDAvailable(uint iArchID)
	{
		for (map<uint, ITEM_INFO>::iterator iter = mapAvailableItems.begin();
			iter != mapAvailableItems.end();
			iter++)
		{
			if (iter->second.iArchID == iArchID)
				return true;
		}
		return false;
	}

	wstring GetItemDescription(uint iArchID)
	{
		for (map<uint, ITEM_INFO>::iterator iter = mapAvailableItems.begin();
			iter != mapAvailableItems.end();
			iter++)
		{
			if (iter->second.iArchID == iArchID)
				return iter->second.wscDescription;
		}
		return L"";
	}

	void PimpShip::LoadSettings(const string &scPluginCfgFile)
	{
		set_iCost = 0;
		mapAvailableItems.clear();
		set_mapDealers.clear();

		// Patch BaseDataList::get_room_data to suppress annoying warnings flserver-errors.log
		unsigned char patch1[] = { 0x90, 0x90 };
		WriteProcMem((char*)0x62660F2, &patch1, 2);

		int iItemID = 1;
		INI_Reader ini;
		if (ini.open(scPluginCfgFile.c_str(), false))
		{
			while (ini.read_header())
			{
				if (ini.is_header("ShipPimper"))
				{
					while (ini.read_value())
					{
						if (ini.is_value("cost"))
						{
							set_iCost = ini.get_value_int(0);
						}
						else if (ini.is_value("equip"))
						{
							string nickname = ini.get_value_string(0);
							string description = ini.get_value_string(1);
							uint iArchID = CreateID(nickname.c_str());
							mapAvailableItems[iItemID].iArchID = iArchID;
							mapAvailableItems[iItemID].wscNickname = stows(nickname);
							mapAvailableItems[iItemID].wscDescription = stows(description);
							if (mapAvailableItems[iItemID].wscDescription.length() == 0)
								mapAvailableItems[iItemID].wscDescription = mapAvailableItems[iItemID].wscNickname;
							iItemID++;
						}
						else if (ini.is_value("room"))
						{
							string nickname = ini.get_value_string(0);
							uint iLocationID = CreateID(nickname.c_str());
							if (!BaseDataList_get()->get_room_data(iLocationID))
							{
								if (set_iPluginDebug>0)
								{
									ConPrint(L"NOTICE: Room %s does not exist\n", stows(nickname).c_str());
								}
							}
							else
							{
								set_mapDealers[iLocationID] = stows(nickname);
							}
						}
					}
				}
			}
			ini.close();
		}

		// Unpatch BaseDataList::get_room_data to suppress annoying warnings flserver-errors.log
		unsigned char unpatch1[] = { 0xFF, 0x12 };
		WriteProcMem((char*)0x62660F2, &patch1, 2);
	}

	// On entering a room check to see if we're in a valid ship dealer room (or base if a 
	// ShipDealer is not defined). If we are then print the intro text otherwise do
	// nothing.
	void PimpShip::LocationEnter(unsigned int iLocationID, unsigned int iClientID)
	{
		if (!set_bEnablePimpShip)
			return;

		if (set_mapDealers.find(iLocationID)==set_mapDealers.end())
		{
			uint iBaseID = 0;
			pub::Player::GetBase(iClientID, iBaseID);
			if (set_mapDealers.find(iBaseID)==set_mapDealers.end())
			{
				mapInfo[iClientID].bInPimpDealer = false;
				mapInfo[iClientID].mapCurrEquip.clear();
				return;
			}
		}

		mapInfo[iClientID].bInPimpDealer = true;

		// Build the equipment list.
		int iSlotID = 1;

		list<EquipDesc> &eqLst = Players[iClientID].equipDescList.equip;
		for (list<EquipDesc>::iterator eq = eqLst.begin(); eq != eqLst.end(); eq++)
		{
			if (IsItemArchIDAvailable(eq->iArchID))
			{
				mapInfo[iClientID].mapCurrEquip[iSlotID].sID = eq->sID;
				mapInfo[iClientID].mapCurrEquip[iSlotID].iArchID = eq->iArchID;
				mapInfo[iClientID].mapCurrEquip[iSlotID].wscHardPoint = stows(eq->szHardPoint.value);
				iSlotID++;
			}
		}

		if (set_wscIntroMsg1.length() > 0)
			PrintUserCmdText(iClientID, L"%s", set_wscIntroMsg1.c_str());

		if (set_wscIntroMsg2.length() > 0)
			PrintUserCmdText(iClientID, L"%s", set_wscIntroMsg2.c_str());
	}

	bool PimpShip::UserCmd_PimpShip(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
	{
		if (!set_bEnablePimpShip)
			return false;

		if (!mapInfo[iClientID].bInPimpDealer)
		{
			PrintUserCmdText(iClientID, L"ERR Pimpship facilities are not available here");
			return true;
		}

		PrintUserCmdText(iClientID, L"Available ship pimping commands:");
		PrintUserCmdText(iClientID, L"This facility costs " + ToMoneyStr(set_iCost) + L" credits to use per one item.");

		PrintUserCmdText(iClientID, L"/showsetup");
		PrintUserCmdText(iClientID, L"|     Display current ship setup.");

		PrintUserCmdText(iClientID, L"/showitems [from]-[to]");
		PrintUserCmdText(iClientID, L"|     Display items that may be added to your ship.");

		PrintUserCmdText(iClientID, L"/setitem <hardpoint id> <item id>");
		PrintUserCmdText(iClientID, L"|     Change the item at <hardpoint id> to <item id>.");
		PrintUserCmdText(iClientID, L"|     <hardpoint id>s are shown by typing /show setup.");
		PrintUserCmdText(iClientID, L"|     <item id>s are shown by typing /show items.");
		PrintUserCmdText(iClientID, L"|     Allowed to print item name instead of ID.");

		PrintUserCmdText(iClientID, L"/setitem [hp1]-[hp-2] <item id>");
		PrintUserCmdText(iClientID, L"|     Change items at hardpoints in range of <hp1>-<hp2> to <item id>.");

		PrintUserCmdText(iClientID, L"/setitem [hp-begin]*<every-n>*[hp-end] <item id>");
		PrintUserCmdText(iClientID, L"|     Change every n hardpoint to <item id>.");
		PrintUserCmdText(iClientID, L"|     Changing begins from [hp-begin] if the parameter is determined, otherwise begins from first hardpoint.");
		PrintUserCmdText(iClientID, L"|     Changing ends at [hp-end] if the parameter is determined, otherwise ends at last hardpoint.");

		return true;
	}

	/// Show the setup of the player's ship.
	bool PimpShip::UserCmd_ShowSetup(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
	{
		if (!mapInfo[iClientID].bInPimpDealer || !set_bEnablePimpShip)
			return false;

		PrintUserCmdText(iClientID, L"Current ship setup: %d", mapInfo[iClientID].mapCurrEquip.size());
		for (map<uint, EQ_HARDPOINT>::iterator iter = mapInfo[iClientID].mapCurrEquip.begin();
			iter != mapInfo[iClientID].mapCurrEquip.end();
			iter++)
		{
			PrintUserCmdText(iClientID, L"|     %.2d | %s : %s",
				iter->first, iter->second.wscHardPoint.c_str(), GetItemDescription(iter->second.iArchID).c_str());
		}
		PrintUserCmdText(iClientID, L"OK");
		return true;
	}

	/// Show the items that may be changed.
	bool PimpShip::UserCmd_ShowItems(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
	{
		if (!mapInfo[iClientID].bInPimpDealer || !set_bEnablePimpShip)
			return false;

		int beginFrom = 1;
		int endAt = mapAvailableItems.size();

		ushort index = 0;
		for (wstring::const_iterator it = wscParam.begin(); it != wscParam.end(); it++)
		{
			index++;

			if (*it == '-')
			{
				if (it == wscParam.begin() && it == wscParam.end() - 1)
				{
					beginFrom = 1;
					endAt = mapInfo[iClientID].mapCurrEquip.size();
				}
				else if (it == wscParam.end() - 1)
				{
					beginFrom = ToInt(wscParam.substr(0, index));
					endAt = mapInfo[iClientID].mapCurrEquip.size();
				}
				else if (it == wscParam.begin())
				{
					beginFrom = 1;
					endAt = ToInt(wscParam.substr(index));

					if (endAt < beginFrom)
						swap(endAt, beginFrom);
				}
				else
				{
					beginFrom = ToInt(wscParam.substr(0, index));
					endAt = ToInt(wscParam.substr(index));

					if (endAt < beginFrom)
						swap(endAt, beginFrom);
				}

				break;
			}
		}

		if ((uint)endAt > mapAvailableItems.size())
			endAt = mapAvailableItems.size();

		if (beginFrom == 0)
			beginFrom = 1;

		if (beginFrom > endAt)
			beginFrom = endAt + 1;

		PrintUserCmdText(iClientID, L"Showed %i/%u items:", endAt - beginFrom + 1, mapAvailableItems.size());
		for (int i = beginFrom; i != endAt + 1; i++)
		{
			PrintUserCmdText(iClientID, L"|     %.2d:  %s", i, mapAvailableItems[i].wscDescription.c_str());
		}
		PrintUserCmdText(iClientID, L"OK");

		return true;
	}

	/// Change the item on the Slot ID to the specified item.
	bool PimpShip::UserCmd_ChangeItem(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
	{
		if (!mapInfo[iClientID].bInPimpDealer || !set_bEnablePimpShip)
			return false;

		int beginFrom = 0;
		int endAt = 0;
		int everyN = 1;

		wstring firstArg = GetParam(wscParam, ' ', 0);
		wstring secondArg = GetParam(wscParam, ' ', 1);

		uint itemID = ToInt(secondArg);

		if (!itemID)
		{
			for (map<uint, ITEM_INFO>::iterator it = mapAvailableItems.begin(); it != mapAvailableItems.end(); it++)
			{
				if (it->second.wscNickname == secondArg)
				{
					itemID = it->first;
					break;
				}
			}
		}

		if (mapAvailableItems.find(itemID) == mapAvailableItems.end())
		{
			PrintUserCmdText(iClientID, L"ERR Invalid item ID");
			return true;
		}

		ushort index = 0;
		for (wstring::iterator it = firstArg.begin(); it != firstArg.end(); it++)
		{
			index++;

			if (*it >= '0' && *it <= '9') {}
			else if (*it == '-')
			{
				if (it == firstArg.begin() && it == firstArg.end() - 1)
				{
					beginFrom = 1;
					endAt = mapInfo[iClientID].mapCurrEquip.size();
				}
				else if (it == firstArg.end() - 1)
				{
					beginFrom = ToInt(firstArg.substr(0, index));
					endAt = mapInfo[iClientID].mapCurrEquip.size();
				}
				else if (it == firstArg.begin())
				{
					beginFrom = 1;
					endAt = ToInt(firstArg.substr(index));

					if (endAt < beginFrom)
						swap(endAt, beginFrom);
				}
				else
				{
					beginFrom = ToInt(firstArg.substr(0, index));
					endAt = ToInt(firstArg.substr(index));

					if (endAt < beginFrom)
						swap(endAt, beginFrom);
				}

				break;
			}
			else if (*it == '*')
			{
				if (beginFrom == 0)
				{
					endAt = mapInfo[iClientID].mapCurrEquip.size();

					if (it == firstArg.begin())
					{
						beginFrom = 1;
						everyN = ToInt(firstArg.substr(index));
					}
					else if (it == firstArg.end() - 1)
					{
						beginFrom = 1;
						everyN = ToInt(firstArg.substr(0, index));
					}
					else
					{
						beginFrom = ToInt(firstArg.substr(0, index));
						everyN = ToInt(firstArg.substr(index));

						if (beginFrom == 0)
							beginFrom = -1;
					}
				}
				else
				{
					if (it == firstArg.end() - 1)
						endAt = mapInfo[iClientID].mapCurrEquip.size();
					else
					{
						endAt = ToInt(firstArg.substr(index));

						if (endAt < beginFrom)
							swap(endAt, beginFrom);
					}

					break;
				}
			}
			else
			{
				PrintUserCmdText(iClientID, L"ERR Invalid syntax");
				return true;
			}
		}

		if (beginFrom == 0 && endAt == 0)
		{
			beginFrom = endAt = ToInt(firstArg);
			if (beginFrom == 0 || (uint)beginFrom > mapInfo[iClientID].mapCurrEquip.size())
			{
				PrintUserCmdText(iClientID, L"ERR hardpoint index is out of bounds");
				return true;
			}
		}

		if (beginFrom <= 0 || (uint)beginFrom > mapInfo[iClientID].mapCurrEquip.size())
		{
			PrintUserCmdText(iClientID, L"ERR Beginning is out of bounds");
			PrintUserCmdText(iClientID, L"You may want to use following syntax to select all hardpoints from beginning to %u:", endAt);
			if (everyN == 1)
				PrintUserCmdText(iClientID, L"/setitem -%i %u", endAt, itemID);
			else
				PrintUserCmdText(iClientID, L"/setitem *%i*%i %u", everyN, endAt, itemID);

			return true;
		}

		if ((uint)endAt > mapInfo[iClientID].mapCurrEquip.size())
		{
			PrintUserCmdText(iClientID, L"ERR Ending is out of bounds");
			PrintUserCmdText(iClientID, L"You may want to use following syntax to select all hardpoints from %u to end:", beginFrom);
			if (everyN == 1)
				PrintUserCmdText(iClientID, L"/setitem %i- %u", beginFrom, itemID);
			else
				PrintUserCmdText(iClientID, L"/setitem %i*%i* %u", beginFrom, everyN, itemID);
			return true;
		}

		if (everyN == 0)
		{
			PrintUserCmdText(iClientID, L"ERR Zero advancement");
			return true;
		}

		int totalCost = 0;
		map<uint, EQ_HARDPOINT>& info = mapInfo[iClientID].mapCurrEquip;
		uint newItem = mapAvailableItems[itemID].iArchID;
		for (int i = beginFrom; i < endAt + 1; i += everyN)
		{
			if (info[i].iArchID != newItem)
				totalCost += set_iCost;
		}

		if (totalCost == 0)
		{
			PrintUserCmdText(iClientID, L"You already have this light mounted at selected hardpoints.");
			return true;
		}

		int iCash = 0;
		wstring wscCharName = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
		HkGetCash(wscCharName, iCash);

		if (iCash < totalCost)
		{
			PrintUserCmdText(iClientID, L"ERR Insufficient credits");
			return true;
		}

		list<EquipDesc> &equip = Players[iClientID].equipDescList.equip;
		for (list<EquipDesc>::iterator it = equip.begin(); it != equip.end(); it++)
		{
			for (int i = beginFrom; i < endAt + 1; i += everyN)
			{
				if (it->sID == info[i].sID)
				{
					it->iArchID = newItem;
					info[i].iArchID = newItem;
					break;
				}
			}
		}

		HkSetEquip(iClientID, equip);
		HkAddCash(wscCharName, 0 - totalCost);
		PrintUserCmdText(iClientID, L"Ship pimping complete. You bought %i item%ws.", endAt - beginFrom + 1, endAt == beginFrom ? L"" : L"s");

		if (beginFrom == 1 && endAt == mapInfo[iClientID].mapCurrEquip.size() && firstArg != L"-")
		{
			PrintUserCmdText(iClientID, L"Next time you may want to use following syntax to select all hardpoints:");
			PrintUserCmdText(iClientID, L"/setitem - %u", itemID);
		}

		pub::Audio::PlaySoundEffect(iClientID, CreateID("ui_execute_transaction"));

		return UserCmd_ShowSetup(iClientID, wscCmd, wscParam, usage);
	}
}
