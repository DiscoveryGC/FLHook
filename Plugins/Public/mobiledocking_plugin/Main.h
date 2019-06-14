#pragma once

#include <windows.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <math.h>
#include <list>
#include <map>
#include <thread>
#include <chrono>
#include <algorithm>
#include <FLHook.h>
#include <plugin.h>
#include <PluginUtilities.h>

struct CLIENT_DATA
{
	uint iDockingModulesInstalled = 0;
	int iDockingModulesAvailable = 0;
	map<wstring, wstring> mapDockedShips;

	// True if currently docked on a carrier
	bool mobileDocked;

	// The name of the carrier
	wstring wscDockedWithCharname;

	// The last real base this ship was on
	uint iLastBaseID;

	// Proxy base in which the ship currently placed. 0 if not.
	uint proxyBaseID = 0;

	Vector carrierPos;
	Matrix carrierRot;
	uint carrierSystem;

	// A base pointer used to teleport the ship into a base on undock
	Universe::IBase *undockBase;
	
	// A flag denoting that the above base should be used as an undock point
	bool baseUndock = false;
};

struct DelayedAction
{
	function<void()> Function;
	chrono::system_clock::time_point expiryTime;
};

extern vector<DelayedAction> TaskScheduler;


struct Task
{
	// Easy create of delayed actions.
	Task(int delayTimeSeconds, function<void()> work)
	{
		DelayedAction action;
		action.Function = work;
		action.expiryTime = chrono::system_clock::now() + chrono::seconds(delayTimeSeconds);
		TaskScheduler.push_back(action);
	}
};

void LoadShip(string shipFileName);
void SaveDockInfoCarrier(const wstring& shipFileName, uint clientID, const CLIENT_DATA& client);
void SaveDockInfoCarried(const wstring& shipFileName, uint clientID, const CLIENT_DATA& client);

void SendResetMarketOverride(uint client);
void SendSetBaseInfoText2(UINT client, const wstring &message);

// Is debug mode running
static int set_iPluginDebug = 1;

// The distance to undock from the carrier
static int set_iMobileDockOffset = 100;

extern map<uint, CLIENT_DATA> mobiledockClients;

// A map of all docking requests pending approval by the carrier
extern map<uint, uint> mapPendingDockingRequests;

