#pragma once

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

struct CLIENT_DATA
{
	int iDockingModulesAvailable = 0;
	uint iDockingModulesInstalled = 0;
	map<wstring, wstring> mapDockedShips;

	// True if currently docked on a carrier
	bool mobileDocked = false;

	// The name of the carrier
	wstring wscDockedWithCharname = L"";

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

struct ActionJettison
{
	int timeLeft;
	wstring carrierCharname;
	wstring dockedCharname;
};

extern vector<ActionJettison> jettisonList;

struct DelayedJettison
{
	DelayedJettison(int delayTimeSecond, wstring carrierCharname, wstring dockedCharname)
	{
		ActionJettison action;
		action.timeLeft = delayTimeSecond;
		action.carrierCharname = carrierCharname;
		action.dockedCharname = dockedCharname;
		jettisonList.push_back(action);
	}
};

void SendResetMarketOverride(uint client);
void SendSetBaseInfoText2(UINT client, const wstring &message);

// The distance to undock from the carrier
static int set_iMobileDockOffset = 100;

extern map<uint, CLIENT_DATA> mobiledockClients;

// A map of all docking requests pending approval by the carrier
extern map<uint, uint> mapPendingDockingRequests;

