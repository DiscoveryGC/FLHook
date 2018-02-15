// Laws.cpp by Laz
// Lists various laws for Sirius' factions.
// Created 28th November 2017
// Last Updated 9th December 2017

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

// Load the configuration

namespace Contraband
{
	static bool set_contrabandEnabled; // The thing that is checked to see if we should use the file. (if yes = apples)

	static map <uint, string> liContrabandItems;
	static map <uint, string> kuContrabandItems;
	static map <uint, string> rhContrabandItems;
	static map <uint, string> brContrabandItems;
	static map <uint, string> gaContrabandItems;
	static map <uint, string> coContrabandItems;
	static map <uint, string> ocContrabandItems;

	static list<uint> liSystems;
	static list<uint> kuSystems;
	static list<uint> rhSystems;
	static list<uint> gaSystems;
	static list<uint> brSystems;
	static list<uint> coSystems;
	static list<uint> ocSystems;

	int ItemsLoaded;
	int ItemsLoaded1;


	void Contraband::LoadSettings(const string &scPluginCfgFile)
	{
		char szCurDir[MAX_PATH];
		GetCurrentDirectory(sizeof(szCurDir), szCurDir);
		string scContrabandPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\laz_laws.cfg";


		// Search for Plugin Configs
		set_contrabandEnabled = IniGetB(scPluginCfgFile, "Config", "Enabled", true);
		//
		INI_Reader ini;
		if (ini.open(scPluginCfgFile.c_str(), false))
		{
			while (ini.read_header())
			{
				if (ini.is_header("Regions"))
				{
					while (ini.read_value())
					{
						if (ini.is_value("li"))
						{
							liSystems.push_back(CreateID(ini.get_value_string(0)));
						}
						else if (ini.is_value("ku"))
						{
							kuSystems.push_back(CreateID(ini.get_value_string(0)));
						}
						else if (ini.is_value("rh"))
						{
							rhSystems.push_back(CreateID(ini.get_value_string(0)));
						}
						else if (ini.is_value("ga"))
						{
							gaSystems.push_back(CreateID(ini.get_value_string(0)));
						}
						else if (ini.is_value("br"))
						{
							brSystems.push_back(CreateID(ini.get_value_string(0)));
						}
						else if (ini.is_value("co"))
						{
							coSystems.push_back(CreateID(ini.get_value_string(0)));
						}
						else if (ini.is_value("oc"))
						{
							ocSystems.push_back(CreateID(ini.get_value_string(0)));
						}
					}
				}
				else if (ini.is_header("Contraband"))
				{
					while (ini.read_value())
					{
						if (ini.is_value("li"))
						{
							liContrabandItems[CreateID(ini.get_value_string(0))] = ini.get_value_string(1);
							ItemsLoaded1++;
						}
						else if (ini.is_value("ku"))
						{
							kuContrabandItems[CreateID(ini.get_value_string(0))] = ini.get_value_string(1);
							ItemsLoaded++;
						}
						else if (ini.is_value("rh"))
						{
							rhContrabandItems[CreateID(ini.get_value_string(0))] = ini.get_value_string(1);
							ItemsLoaded++;
						}
						else if (ini.is_value("ga"))
						{
							gaContrabandItems[CreateID(ini.get_value_string(0))] = ini.get_value_string(1);
							ItemsLoaded++;
						}
						else if (ini.is_value("br"))
						{
							brContrabandItems[CreateID(ini.get_value_string(0))] = ini.get_value_string(1);
							ItemsLoaded++;
						}
						else if (ini.is_value("co"))
						{
							coContrabandItems[CreateID(ini.get_value_string(0))] = ini.get_value_string(1);
							ItemsLoaded++;
						}
						else if (ini.is_value("oc"))
						{
							ocContrabandItems[CreateID(ini.get_value_string(0))] = ini.get_value_string(1);
							ItemsLoaded++;
						}
					}
				}
			}
			ini.close();
			ConPrint(L"CONTRABAND: Found %u illegal items.\n", ItemsLoaded1);
			ConPrint(L"CONTRABAND: Found %u illegal systems.\n", liSystems.size());
		}
	}

	///////////////////////////////////////////////
	//             Functions n'stuff             //
	///////////////////////////////////////////////

	void Contraband::ContrabandWarning(uint iClientID, wstring wscText, ...) // WE WANT RED
	{
		wchar_t wszBuf[1024 * 8] = L"";
		va_list marker;
		va_start(marker, wscText);
		_vsnwprintf(wszBuf, sizeof(wszBuf) - 1, wscText.c_str(), marker);

		wstring wscXML = L"<TRA data=\"65281\" mask=\"-1\"/><TEXT>" + XMLText(wszBuf) + L"</TEXT>";
		HkFMsg(iClientID, wscXML);
	}

	void Contraband::JumpInComplete(unsigned int system, unsigned int ship)
	{
		uint iClientID = HkGetClientIDByShip(ship);
		CheckCargo(iClientID);
	}

	static void Contraband::CheckCargo(int iClientID)
	{
		uint iSystemID;
		pub::Player::GetSystem(iClientID, iSystemID);

		int iHoldSize;
		list<CARGO_INFO> lstCargo;
		HkEnumCargo((const wchar_t*)Players.GetActiveCharacterName(iClientID), lstCargo, iHoldSize);

		// Is this a Liberty System?
		foreach(liSystems, uint, iter)
		{
			ConPrint(L"CONTRABAND: Are we in Liberty?\n");
			if (*iter == iSystemID) // If it is:
			{
				ConPrint(L"CONTRABAND: We are! \n");
				// Look through the list of cargo in our cfg
				for (map<uint, string>::iterator iter = liContrabandItems.begin(); iter != liContrabandItems.end(); iter++)
				{
					ConPrint(L"CONTRABAND: Lets look inside our ships hold!\n");
					uint cargo = iter->first; // Cargo Internal ID
					string cargoName = iter->second; // The String we do in the config
					// Search their cargo to see if it's on the list
					for (list<CARGO_INFO>::iterator i = lstCargo.begin(); i != lstCargo.end(); ++i)
					{
						ConPrint(L"CONTRABAND: Our Cargo is: %u\n", i->iArchID);
						ConPrint(L"CONTRABAND: We want it to be: %u\n", cargo);
						ConPrint(L"CONTRABAND: Cargo in ship /=/ cargo on list\n");
						if (i->iArchID == cargo) // If their cargo is on the list
						{
							ConPrint(L"CONTRABAND: CARDAMINE %s\n", cargoName);
							ContrabandWarning(iClientID, L"WARNING: You are carrying %s which is illegal in this sector.", cargoName);
							PrintUserCmdText(iClientID, L"WARNING: You are carrying %s which is illegal in this sector.", cargoName);
						}
					}
				}
				//PrintUserCmdText(iClientID, L"No apples were harmed in the making of this code.");
			}
		}
	}
		/*foreach(kuSystems, uint, iter)
		{
			if (*iter == iSystemID)
			{
				PrintUserCmdText(iClientID, L"Welcome to Kusari. To view the house laws, type /laws_ku");
			}
		}
		foreach(rhSystems, uint, iter)
		{
			if (*iter == iSystemID)
			{
				PrintUserCmdText(iClientID, L"Welcome to Rheinland. To view the house laws, type /laws_rh");

			}
		}
		foreach(gaSystems, uint, iter)
		{
			if (*iter == iSystemID)
			{
				PrintUserCmdText(iClientID, L"Welcome to Gallia. To view the house laws, type /laws_ga");
			}
		}
		foreach(brSystems, uint, iter)
		{
			if (*iter == iSystemID)
			{
				PrintUserCmdText(iClientID, L"Welcome to Bretonia. To view the house laws, type /laws_br");
			}
		}
		foreach(ocSystems, uint, iter)
		{
			if (*iter == iSystemID)
			{
				PrintUserCmdText(iClientID, L"Welcome to National Council Space, The True House of Hispania. To view the house laws, type /laws_oc");
			}
		}
		foreach(coSystems, uint, iter)
		{
			if (*iter == iSystemID)
			{
				PrintUserCmdText(iClientID, L"You are now in territory of the Corsair Empire. To view the house laws, type /laws_co");
			}
		}
	}*/

	//////////////////////////////////////////////
	//                 Commands                 //
	//////////////////////////////////////////////
	
	// Namespace::FunctionName

/*	bool Contraband::Check(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage)
	{
		//If [Config]
		//Enabled = yes
		//Run code
		if (set_contrabandEnabled)
		{
			
		}
		return true;
	} */
}
