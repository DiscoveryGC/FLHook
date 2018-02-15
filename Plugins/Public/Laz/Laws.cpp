// Laws.cpp by Laz
// Lists various laws for Sirius' factions.
// Created 28th November 2017
// Last Updated 6th Jan 2018

#include <windows.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <io.h>
#include <string>
#include <time.h>
#include <FLHook.h>
#include <plugin.h>
#include <FLCoreServer.h>
#include <FLCoreCommon.h>
#include <PluginUtilities.h>
#include <math.h>
#include <map>
#include <list>
#include <vector>
#include "Main.h"

#define POPUPDIALOG_BUTTONS_LEFT_YES 1
#define POPUPDIALOG_BUTTONS_CENTER_NO 2
#define POPUPDIALOG_BUTTONS_RIGHT_LATER 4
#define POPUPDIALOG_BUTTONS_CENTER_OK 8
// Load the configuration

namespace Laws
{
	static bool set_lawsEnabled;
	static bool set_lawsJumpEnabled;
	static bool set_lawsContrabandEnabled;

	list<INISECTIONVALUE> lstHouseTag;
	list<string> lstHouse;

	map<uint, string> mapHouseAndSystems;
	map<string, wstring> mapHouseLawList;
	map<string, list<uint>> mapHouseCargoList;

	uint PrevSystemID;

	void Laws::LoadSettings(const string &scPluginCfgFile)
	{
		int housesLoaded = 0;
		int houseSystemsLoaded = 0;
		int lawsLoaded = 0;
		int contrabandLoaded = 0;

		char szCurDir[MAX_PATH];
		GetCurrentDirectory(sizeof(szCurDir), szCurDir);
		string scLawsPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\laz_laws.cfg";
		string scSelectEquip = "..\\data\\equipment\\select_equip.ini";

		set_lawsEnabled = IniGetB(scPluginCfgFile, "LawsConfig", "Enabled", true);
		set_lawsJumpEnabled = IniGetB(scPluginCfgFile, "JumpNotifications", "Enabled", true);
		set_lawsContrabandEnabled = IniGetB(scPluginCfgFile, "ContrabandConfig", "Enabled", true);

		IniGetSection(scPluginCfgFile, "Houses", lstHouseTag);

		foreach(lstHouseTag, INISECTIONVALUE, iter)
		{
			lstHouse.push_back(iter->scKey);
			housesLoaded++;
		}

		INI_Reader ini;
		if (ini.open(scPluginCfgFile.c_str(), false))
		{
				while (ini.read_header())
				{
					foreach(lstHouse, string, iter)
					{
						string scIter = *iter;
						if (ini.is_header(scIter.c_str()))
						{
							while (ini.read_value())
							{
								if (ini.is_value("system"))
								{
									mapHouseAndSystems[CreateID(ini.get_value_string(0))] = scIter;
									houseSystemsLoaded++;
								}
								else if (ini.is_value("contraband"))
								{
									mapHouseCargoList[scIter].push_back(CreateID(ini.get_value_string(0)));
									contrabandLoaded++;
								}
							}
						}
						else if (ini.is_header("Laws"))
						{
							while (ini.read_value())
							{
								if (ini.is_value("house"))
								{
									mapHouseLawList[ini.get_value_string(0)] = stows(ini.get_value_string(1));
									lawsLoaded++;
								}
							}
						}
					}
				}
			ini.close();

			ConPrint(L"LAWS: Houses Loaded: %u \n", housesLoaded);
			ConPrint(L"LAWS: House Systems Loaded: %u \n", houseSystemsLoaded);
			ConPrint(L"LAWS: Laws Loaded: %u \n", lawsLoaded);
			ConPrint(L"LAWS: Contraband Items Loaded: %u \n", contrabandLoaded);
		}
	}

	///////////////////////////////////////////////
	//             Functions n'stuff             //
	///////////////////////////////////////////////

	void Laws::ContrabandWarning(uint iClientID, wstring wscText, ...) // WE WANT BOLD RED
	{
		wchar_t wszBuf[1024 * 8] = L"";
		va_list marker;
		va_start(marker, wscText);
		_vsnwprintf(wszBuf, sizeof(wszBuf) - 1, wscText.c_str(), marker);

		wstring wscXML = L"<TRA data=\"65281\" mask=\"-1\"/><TEXT>" + XMLText(wszBuf) + L"</TEXT>";
		HkFMsg(iClientID, wscXML);
	}

	static void Laws::CheckCargo(int iClientID)
	{
		if (set_lawsContrabandEnabled)
		{
			uint iSystemID;
			pub::Player::GetSystem(iClientID, iSystemID);

			int iHoldSize;
			list<CARGO_INFO> lstCargo;
			HkEnumCargo((const wchar_t*)Players.GetActiveCharacterName(iClientID), lstCargo, iHoldSize);

			for (map<uint, string>::iterator iter = mapHouseAndSystems.begin(); iter != mapHouseAndSystems.end(); iter++)
			{

				uint currentSector = iter->first;
				string currentHouse = iter->second;

				if (currentSector == iSystemID)
				{
					for (map<string, list<uint>>::iterator iter = mapHouseCargoList.begin(); iter != mapHouseCargoList.end(); iter++)
					{
						string currentHouseCompare = iter->first;
						list<uint> cfgCargoLst = iter->second;

						if (currentHouse == currentHouseCompare)
						{
							for (list<CARGO_INFO>::iterator i = lstCargo.begin(); i != lstCargo.end(); ++i)
							{
								uint correctCargo = i->iArchID;
								foreach(cfgCargoLst, uint, cfgCargoIter)
								{
									if (*cfgCargoIter == correctCargo) // If their cargo is on the list
									{
										ContrabandWarning(iClientID, L"WARNING: You are carrying items which are illegal in this sector.");
										return;
									}
								}
							}
						}
					}
					//PrintUserCmdText(iClientID, L"No apples were harmed in the making of this code.");
				}
			}
		}
	}

	void Laws::PlayerLaunch(uint iShip, unsigned int iClientID)
	{
		pub::Player::GetSystem(iClientID, PrevSystemID);
		CheckSector(iClientID);
		CheckCargo(iClientID);
	}

	void Laws::JumpInComplete(unsigned int system, unsigned int ship)
	{
		uint iClientID = HkGetClientIDByShip(ship);
		CheckSector(iClientID);
		CheckCargo(iClientID);
	}

	// Checking System @ Jumpin
	static void Laws::CheckSector(int iClientID) 
	{
		if (set_lawsJumpEnabled)
		{
			uint currentsystem;
			pub::Player::GetSystem(iClientID, currentsystem);

			for (map<uint, string>::iterator iter = mapHouseAndSystems.begin(); iter != mapHouseAndSystems.end(); iter++)
			{
				uint currentSystem = iter->first;
				string currentHouse = iter->second;
				if (currentSystem == currentsystem && currentSystem != PrevSystemID)
				{
					PrintUserCmdText(iClientID, L"Welcome to %s. To view the local laws, type /laws", stows(currentHouse).c_str());
					pub::Player::GetSystem(iClientID, PrevSystemID);
				}
				else
				{
					pub::Player::GetSystem(iClientID, PrevSystemID);
				}
			}
		}
	}

	//////////////////////////////////////////////
	//                 Commands                 //
	//////////////////////////////////////////////


	bool Laws::UserCmd_Laws(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage)
	{
		if (set_lawsEnabled)
		{
			for (map<uint, string>::iterator iter = mapHouseAndSystems.begin(); iter != mapHouseAndSystems.end(); iter++)
			{
				uint currentSystem = iter->first;
				string currentHouse = iter->second;

				uint getSystem;
				pub::Player::GetSystem(iClientID, getSystem);

				if (currentSystem == getSystem)
				{
					for (map<string, wstring>::iterator iter2 = mapHouseLawList.begin(); iter2 != mapHouseLawList.end(); iter2++)
					{
						string currentHouseCompare = iter2->first;
						if (currentHouse == currentHouseCompare)
						{
							wstring scXML = iter2->second;

							wstring wscPlayerInfo = L"<RDL><PUSH/>" + scXML + L"<PARA/><POP/></RDL>";

							struct PlayerData *pPD = 0;
							while (pPD = Players.traverse_active(pPD))
							{
								uint iClientID = HkGetClientIdFromPD(pPD);

								HkChangeIDSString(iClientID, 66076, wscPlayerInfo);
								PrintUserCmdText(iClientID, L"Please press F5 to access the laws menu. (Open the Neural Net. This will not work if you are currently on a mission)");

								FmtStr caption(0, 0);
								caption.begin_mad_lib(1);
								caption.end_mad_lib();

								FmtStr message(0, 0);
								message.begin_mad_lib(66076);
								message.end_mad_lib();

								pub::Player::PopUpDialog(iClientID, caption, message, POPUPDIALOG_BUTTONS_CENTER_OK);
							}
						}
					}

				}
			}
			return false;
		}
		return true;
	}
}
