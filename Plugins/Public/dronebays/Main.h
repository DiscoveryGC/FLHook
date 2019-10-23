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

	// The ship archetype which this drone uses - Defaults to civilian bomber
	uint archetype = CreateID("dsy_civbomb");

	// The loadout identification which this drone uses - Defaults to no loadout
	uint loadout = CreateID("null_loadout");
};

struct BayArch
{
	// How long this bay takes to launch a drone
	int iDroneBuildTime;

	// What is the maximum range that a drone can be from this bay, without being recalled (Defaults to 10k)
	uint droneRange = 10000;

	// A list of available drone aliases usable by this bayarch
	vector<string> availableDrones;

	// A list defining what allowed ships this bayarch can target
	vector<int> validShipclassTargets;
};

struct DeployedDroneInfo
{
	uint deployedDroneObj;
	uint lastShipObjTarget = 0;
};

struct ClientDroneInfo
{
	// The NPC identification for the deployed ship
	DeployedDroneInfo deployedInfo;

	// The production state of the drone bay
	BUILD_STATE buildState = STATE_DRONE_OFF;

	// The type of DroneBay equipped by the client
	BayArch droneBay;

	// The carrier shipObj
	uint carrierShipobj;
};

struct ChatDebounceStruct
{
	// The last drone which was targeted by the user - This is used to prevent duplicate messages on drone deselect
	uint lastDroneObjSelected;

	// Have we recently printed out a drone name before?
	bool debounceToggle = false;
};


// A wrapper struct used to encapsulate all of the information the plugin needs to remember while in the buildstate
struct DroneBuildTimerWrapper
{
	mstime startBuildTime;
	DroneArch reqDrone;
	int buildTimeRequired;
};

struct DroneDespawnWrapper
{
	uint droneObj;
	uint parentObj;

	int timeElapsedSinceRecallCmd = 0;
};

extern map<uint, DroneBuildTimerWrapper> buildTimerMap;
extern map<uint, DroneDespawnWrapper> droneDespawnMap;
extern map<uint, ClientDroneInfo> clientDroneInfo;

extern map<uint, BayArch> availableDroneBays;
extern map<string, DroneArch> availableDroneArch;

/*
 * Messages alerting a user of who owns a drone fires >5 times for some reason on first select.
 * Maintain a map cleared every 2 seconds which makes sure you only see the name once per click
 */
extern map<uint, ChatDebounceStruct> droneAlertDebounceMap;

namespace UserCommands
{
	bool UserCmd_Deploy(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
	bool UserCmd_AttackTarget(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
	bool UserCmd_RecallDrone(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
	bool UserCmd_Debug(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
	bool UserCmd_DroneStop(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
	bool UserCmd_DroneCome(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
	bool UserCmd_DroneHelp(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
	bool UserCmd_DroneBayAvailability(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
}

namespace Utility
{
	void DeployDrone(uint iClientID, const DroneBuildTimerWrapper& timerWrapper);
	float RandFloatRange(float a, float b);
	void CreateNPC(uint iClientID, Vector pos, Matrix rot, uint iSystem, DroneArch drone);
	pub::AI::SetPersonalityParams MakePersonality();

	void SetRepNeutral(uint clientObj, uint targetObj);
	void SetRepHostile(uint clientObj, uint targetObj);

	uint CreateDroneNameInfocard(const uint& droneOwnerId);

	void LogEvent(const char *szString, ...);
}

namespace Timers
{
	void processDroneMaxDistance(map<uint, ClientDroneInfo>& clientDrones);
	void processDroneDockRequests(map<uint, DroneDespawnWrapper>& despawnList);
	void processDroneBuildRequests(map<uint, DroneBuildTimerWrapper>& buildList);
}

#endif
