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
	uint iDockingModulesInstalled = 0;
	int iDockingModulesAvailable = 0;
	map<wstring, wstring> mapDockedShips;

	// True if currently docked on a carrier
	bool mobileDocked;

	// The name of the carrier
	wstring wscDockedWithCharname;

	// The last real base this ship was on
	uint iLastBaseID;

	Vector carrierPos;
	Matrix carrierRot;
	uint carrierSystem;

	// A base pointer used to teleport the ship into a base on undock
	Universe::IBase *undockBase;
	
	// A flag denoting that the above base should be used as an undock point
	bool baseUndock = false;
};

struct DEFERREDJUMPS
{
	uint system;
	Vector pos;
	Matrix rot;
};

static map<uint, DEFERREDJUMPS> mapDeferredJumps;

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

