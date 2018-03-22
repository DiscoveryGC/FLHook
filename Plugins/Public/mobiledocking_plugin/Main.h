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
	
	bool bSetup;

	bool reverse_sell;
	bool stop_buy;
	list<CARGO_INFO> cargo;

	bool bAdmin;

	uint iDockingModules;
	map<wstring, wstring> mapDockedShips;

	// True if currently docked on a carrier
	bool mobileDocked;

	// Boolean flag used for a within-system teleport, instead of a system changeout
	bool simpleJump = false;

	// The name of the carrier
	wstring wscDockedWithCharname;

	// The last real base this ship was on
	uint iLastBaseID;

	// The last system which the vessel was in
	uint iLastSystem;

	Vector carrierPos;
	Matrix carrierRot;
	uint carrierSystem;
};

struct DEFERREDJUMPS
{
	uint system;
	Vector pos;
	Matrix rot;
};

static map<uint, DEFERREDJUMPS> mapDeferredJumps;

void LoadDockInfo(uint client);
void SaveDockInfo(uint client, CLIENT_DATA clientData);

void SendResetMarketOverride(uint client);
void SendSetBaseInfoText2(UINT client, const wstring &message);

// Is debug mode running
static int set_iPluginDebug = 1;

// The distance to undock from the carrier
static int set_iMobileDockOffset = 100;

extern map<uint, CLIENT_DATA> mobiledockClients;

// A map of all docking requests pending approval by the carrier
extern map<uint, uint> mapPendingDockingRequests;

