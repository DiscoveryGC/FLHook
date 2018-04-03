// Template for FLHookPlugin
// February 2016 by BestDiscoveryHookDevs2016
//
// This is a template with the bare minimum to have a functional plugin.
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
#include <list>
#include <map>
#include <algorithm>
#include <FLHook.h>
#include <plugin.h>
#include <PluginUtilities.h>
#include "Main.h"

#include "../hookext_plugin/hookext_exports.h"

static int set_iPluginDebug = 0;

/// A return code to indicate to FLHook if we want the hook processing to continue.
PLUGIN_RETURNCODE returncode;

void LoadSettings();

map<uint, DroneBuildTimerWrapper> buildTimerMap;
map<uint, DroneDespawnWrapper> droneDespawnMap;
map<uint, ClientDroneInfo> clientDroneInfo;

map<uint, BayArch> availableDroneBays;
map<string, DroneArch> availableDroneArch;
vector<uint> npcnames;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	srand((uint)time(0));
	// If we're being loaded from the command line while FLHook is running then
	// set_scCfgFile will not be empty so load the settings as FLHook only
	// calls load settings on FLHook startup and .rehash.
	if(fdwReason == DLL_PROCESS_ATTACH)
	{
		if (set_scCfgFile.length()>0)
			LoadSettings();
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
	}
	return true;
}

/// Hook will call this function after calling a plugin function to see if we the
/// processing to continue
EXPORT PLUGIN_RETURNCODE Get_PluginReturnCode()
{
	return returncode;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Loading Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	string File_FLHook = R"(..\exe\flhook_plugins\dronebays.cfg)";
	int loadedBays = 0;
	int loadedDrones = 0;

	INI_Reader ini;
	if (ini.open(File_FLHook.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("Names"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("name"))
					{
						npcnames.push_back(ini.get_value_int(0));
					}
				}
			}
			else if (ini.is_header("BayArch"))
			{
				BayArch bayArch;
				uint bayEquipId = 0;

				while (ini.read_value())
				{
					if (ini.is_value("equipmentId"))
					{
						bayEquipId = CreateID(ini.get_value_string(0));
					}
					else if (ini.is_value("launchtime"))
					{
						bayArch.iDroneBuildTime = ini.get_value_int(0);
					}
					else if (ini.is_value("availabledrone"))
					{
						bayArch.availableDrones.emplace_back(ini.get_value_string(0));
					}
				}
				if(bayEquipId != 0)
				{
					availableDroneBays[bayEquipId] = bayArch;
					loadedBays++;
				}
			}
			else if (ini.is_header("DroneArch"))
			{
				DroneArch droneArch;
				string aliasName;
				while (ini.read_value())
				{
					if (ini.is_value("aliasName"))
					{
						aliasName = ini.get_value_string(0);
					}
					else if (ini.is_value("archetype"))
					{
						droneArch.archetype = CreateID(ini.get_value_string(0));
					}
					else if (ini.is_value("loadout"))
					{
						droneArch.loadout = CreateID(ini.get_value_string(0));
					}
				}
				if(!aliasName.empty())
				{
					availableDroneArch[aliasName] = droneArch;
					loadedDrones++;
				}
			}
		}
		ini.close();
	}

	//@@TODO: Verify that all of the aliased drone names with the bayarches match a drone config.

	ConPrint(L"DRONEBAY: %i bayarches loaded.\n", loadedBays);
	ConPrint(L"DRONEBAY: %i dronearches loaded.\n", loadedDrones);
	

}

/** Clean up when a client disconnects */
void ClearClientInfo(uint iClientID)
{
	// If a drone exists for the user, destroy it
	if(clientDroneInfo[iClientID].deployedInfo.deployedDroneObj != 0)
	{
		pub::SpaceObj::Destroy(clientDroneInfo[iClientID].deployedInfo.deployedDroneObj, DestroyType::FUSE);
	}
	
	// Erase any struct entries
	buildTimerMap.erase(iClientID);
	droneDespawnMap.erase(iClientID);
	clientDroneInfo.erase(iClientID);


}

void __stdcall ShipDestroyed(DamageList *_dmg, DWORD *ecx, uint iKill)
{
	returncode = DEFAULT_RETURNCODE;
	if (iKill)
	{
		CShip *cship = reinterpret_cast<CShip*>(ecx[4]);
		
		// Check if this is a drone or carrier being destroyed
		for (auto drone = clientDroneInfo.begin(); drone != clientDroneInfo.end(); ++drone)
		{
			if(cship->get_id() == drone->second.deployedInfo.deployedDroneObj)
			{
				// If so, clear the carriers map and alert them
				clientDroneInfo.erase(drone->first);
				clientDroneInfo[drone->first].buildState = STATE_DRONE_OFF;

				PrintUserCmdText(drone->first, L"Drone has been destroyed.");
			}

			// If the carrier is being destroyed, destroy the drone as well
			else if(cship->get_id() == drone->second.carrierShipobj)
			{
				if(!drone->second.deployedInfo.deployedDroneObj != 0)
				{
					pub::SpaceObj::Destroy(drone->second.deployedInfo.deployedDroneObj, DestroyType::FUSE);
					clientDroneInfo.erase(drone->first);
					clientDroneInfo[drone->first].buildState = STATE_DRONE_OFF;

					PrintUserCmdText(drone->first, L"Drone has been destroyed.");

				}
			}
		}
	}
}

void __stdcall SystemSwitchOutComplete(unsigned int iShip, unsigned int iClientID)
{
	// If the carrier changes systems, destroy any old drones
	if(clientDroneInfo[iClientID].deployedInfo.deployedDroneObj != 0)
	{
		pub::SpaceObj::Destroy(clientDroneInfo[iClientID].deployedInfo.deployedDroneObj, DestroyType::FUSE);
		PrintUserCmdText(iClientID, L"Drones cannot handle the tear of jumping. Self-destructing.");

		clientDroneInfo.erase(iClientID);
		clientDroneInfo[iClientID].buildState = STATE_DRONE_OFF;
	}
}

void __stdcall BaseEnter_AFTER(unsigned int iBaseID, unsigned int iClientID)
{
	// If the carrier changes systems, destroy any old drones
	if (clientDroneInfo[iClientID].deployedInfo.deployedDroneObj != 0)
	{
		pub::SpaceObj::Destroy(clientDroneInfo[iClientID].deployedInfo.deployedDroneObj, DestroyType::FUSE);
		PrintUserCmdText(iClientID, L"Drones cannot dock or land on non-carrier structures. Self-destructing.");

		clientDroneInfo.erase(iClientID);
		clientDroneInfo[iClientID].buildState = STATE_DRONE_OFF;
	}
}

void HkTimerCheckKick()
{
	const mstime now = timeInMS();
	uint curr_time = static_cast<uint>(time(nullptr));

	// Check the launch timers for each client
	for (auto dt = buildTimerMap.begin(); dt != buildTimerMap.end(); ++dt)
	{
		if ((dt->second.startBuildTime + (dt->second.buildTimeRequired * 1000)) < now)
		{
			Utility::DeployDrone(dt->first, dt->second);
			buildTimerMap.erase(dt);
		}
	}

	// Check the despawn requests for any ships to be removed
	for (auto dt = droneDespawnMap.begin(); dt != droneDespawnMap.end(); ++dt)
	{
		// Get the distance between the two objects and check that it's smaller than the carriers radius
		float carrierRadius;
		Vector radiusVector{};
		pub::SpaceObj::GetRadius(dt->second.parentObj, carrierRadius, radiusVector);
		const int shipDistance = abs(HkDistance3DByShip(dt->second.parentObj, dt->second.droneObj));

		// We give a padding of 100 meters to ensure that there is no collision
		if(shipDistance < (carrierRadius + 100))
		{
			pub::SpaceObj::Destroy(dt->second.droneObj, DestroyType::VANISH);
			PrintUserCmdText(dt->first, L"Drone docked");
			droneDespawnMap.erase(dt->first);
			
			// Rebuild the client dronestate struct from the ground up
			clientDroneInfo.erase(dt->first);
			clientDroneInfo[dt->first].buildState = STATE_DRONE_OFF;
		}
	}

	// Run the move-scanner every ten seconds
	static int moveTimer = 0;
	if(moveTimer == 10)
	{
		for(auto dt = clientDroneInfo.begin(); dt != clientDroneInfo.end(); ++dt)
		{
			// This only matters if the user is in space
			uint carrierShip;
			pub::Player::GetShip(dt->first, carrierShip);

			if(!carrierShip)
				continue;

			// This also only matters if there is a drone to move
			if (clientDroneInfo[dt->first].deployedInfo.deployedDroneObj == 0)
				continue;

			// Check to see if the distance is greater than the maximum distance
			static uint droneSpaceObj = dt->second.deployedInfo.deployedDroneObj;
			static float maxDistanceAllowed = 10000;
			const float distance = HkDistance3DByShip(droneSpaceObj, carrierShip);

			// If the drone is furthur than it should be, retreat to it's owner
			if (distance > maxDistanceAllowed)
			{
				// If the drone is attacking someone, set the reputation back to neutral before returning
				if(dt->second.deployedInfo.lastShipObjTarget != 0)
				{
					Utility::SetRepNeutral(droneSpaceObj, dt->second.deployedInfo.lastShipObjTarget);
				}

				pub::AI::DirectiveGotoOp gotoOp;
				gotoOp.iGotoType = 0;
				gotoOp.iTargetID = carrierShip;
				gotoOp.fRange = 300.0;
				gotoOp.fThrust = 80;
				gotoOp.goto_cruise = true;

				pub::AI::SubmitDirective(droneSpaceObj, &gotoOp);

			}
		}
		moveTimer = 0;
	}
	moveTimer++;
}

void __stdcall CharacterSelect_AFTER(struct CHARACTER_ID const &cId, unsigned int client)
{
	returncode = DEFAULT_RETURNCODE;
	clientDroneInfo[client].buildState = STATE_DRONE_OFF;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Client command processing
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef bool(*_UserCmdProc)(uint, const wstring &, const wstring &, const wchar_t*);

struct USERCMD
{
	wchar_t *wszCmd;
	_UserCmdProc proc;
	wchar_t *usage;
};

USERCMD UserCmds[] =
{
	{ L"/dronedeploy", UserCommands::UserCmd_Deploy, L"Usage: /deploydrone [DroneType]" },
	{ L"/dronedeploy*", UserCommands::UserCmd_Deploy , L"Usage: /deplydrone [DroneType]" },
	{ L"/dronetarget", UserCommands::UserCmd_AttackTarget, L"Usage: Target a vessel and run this command with a drone in space" },
	{ L"/dronedebug", UserCommands::UserCmd_Debug, L"Usage: Git gud" },
	{ L"/dronestop", UserCommands::UserCmd_DroneStop, L"Usage: /dronestop -- This causes the drone to stop whatever it's doing and sit still"},
	{ L"/dronerecall", UserCommands::UserCmd_RecallDrone, L"Usage: /dronerecall"},
	{ L"/dronehelp", UserCommands::UserCmd_DroneHelp, L"Usage: /dronehelp"},
	{ L"/dronetypes", UserCommands::UserCmd_DroneBayAvailability, L"Usage: /dronetypes"},
	{ L"/dronecome", UserCommands::UserCmd_DroneCome, L"Usage: /dronecome"},
};

/**
This function is called by FLHook when a user types a chat string. We look at the
string they've typed and see if it starts with one of the above commands. If it
does we try to process it.
*/
bool UserCmd_Process(uint iClientID, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	wstring wscCmdLineLower = ToLower(wscCmd);

	// If the chat string does not match the USER_CMD then we do not handle the
	// command, so let other plugins or FLHook kick in. We require an exact match
	for (uint i = 0; (i < sizeof(UserCmds) / sizeof(USERCMD)); i++)
	{

		if (wscCmdLineLower.find(UserCmds[i].wszCmd) == 0)
		{
			// Extract the parameters string from the chat string. It should
			// be immediately after the command and a space.
			wstring wscParam = L"";
			if (wscCmd.length() > wcslen(UserCmds[i].wszCmd))
			{
				if (wscCmd[wcslen(UserCmds[i].wszCmd)] != ' ')
					continue;
				wscParam = wscCmd.substr(wcslen(UserCmds[i].wszCmd) + 1);
			}

			// Dispatch the command to the appropriate processing function.
			if (UserCmds[i].proc(iClientID, wscCmd, wscParam, UserCmds[i].usage))
			{
				// We handled the command tell FL hook to stop processing this
				// chat string.
				returncode = SKIPPLUGINS_NOFUNCTIONCALL; // we handled the command, return immediatly
				return true;
			}
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions to hook
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "Drone Bays by Remnant";
	p_PI->sShortName = "dronebay";
	p_PI->bMayPause = false;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;
	
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkTimerCheckKick, PLUGIN_HkTimerCheckKick, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ClearClientInfo, PLUGIN_ClearClientInfo, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ShipDestroyed, PLUGIN_ShipDestroyed, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&BaseEnter_AFTER, PLUGIN_HkIServerImpl_BaseEnter_AFTER, 0));

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterSelect_AFTER, PLUGIN_HkIServerImpl_CharacterSelect_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SystemSwitchOutComplete, PLUGIN_HkIServerImpl_SystemSwitchOutComplete, 0));

	return p_PI;
}

