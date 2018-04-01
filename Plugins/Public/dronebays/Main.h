#ifndef __MAIN_H__
#define __MAIN_H__ 1

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

using namespace std;

enum BUILD_STATE
{
	STATE_DRONE_LAUNCHED = 0,
	STATE_DRONE_OFF = 1,
	STATE_DRONE_BUILDING = 2,
	STATE_DRONE_COMPLETE = 3,
};

struct DroneArch
{
	// The name used to deploy this type of drone
	string aliasName;
	
	// The ship archetype which this drone uses
	uint archetype;

	// The loadout identification which this drone uses
	uint loadout;
};

struct BayArch
{
	// How long this bay takes to launch a drone
	int iDroneBuildTime;

	// A list of available drone aliases usable by this bayarch
	list<string> availableDrones;
};

struct DeployedDroneInfo
{
	uint deployedDroneObj;
	uint lastShipObjTarget;
};

struct ClientDroneInfo
{
	// The NPC identification for the deployed ship
	DeployedDroneInfo deployedInfo;

	// The production state of the drone bay
	BUILD_STATE buildState = STATE_DRONE_OFF;

	// The type of DroneBay equipped by the client
	BayArch droneBay;
};


// A wrapper struct used to encapsulate all of the information the plugin needs to remember while in the buildstate
struct DroneTimerWrapper
{
	mstime startBuildTime;
	DroneArch reqDrone;
	int buildTimeRequired;
};

extern map<uint, DroneTimerWrapper> buildTimerMap;
extern map<uint, ClientDroneInfo> clientDroneInfo;

extern map<uint, BayArch> availableDroneBays;
extern map<string, DroneArch> availableDroneArch;

extern vector<uint> npcnames;

namespace UserCommands
{
	bool UserCmd_Deploy(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
	bool UserCmd_AttackTarget(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
	bool UserCmd_EnterFormation(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
	bool UserCmd_RecallDrone(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
	bool UserCmd_Debug(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
}

namespace Utility
{
	void DeployDrone(uint iClientID, const DroneTimerWrapper& timerWrapper);
	float RandFloatRange(float a, float b);
	void CreateNPC(uint iClientID, Vector pos, Matrix rot, uint iSystem, DroneArch drone);
	pub::AI::SetPersonalityParams MakePersonality();
	uint rand_name();
	
	//@@TODO: Remove the reqClientID from these methods later. They're here for testing purposes.
	void SetRepNeutral(uint clientObj, uint targetObj, uint reqClientId);
	void SetRepHostile(uint clientObj, uint targetObj, uint reqClientId);
}

#endif
