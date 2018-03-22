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
	bool mobile_docked;

	// The name of the carrier
	wstring wscDockedWithCharname;

	// The last known location in space of the carrier
	uint iCarrierSystem;
	Vector vCarrierLocation;
	Matrix mCarrierLocation;

	// The last real base this ship was on
	uint iLastBaseID;
};

struct DEFERREDJUMPS
{
	uint system;
	Vector pos;
	Matrix rot;
};

static map<uint, DEFERREDJUMPS> mapDeferredJumps;

void LoadDockInfo(uint client);
void SaveDockInfo(uint client);
void UpdateDockInfo(const wstring &charname, uint iSystem, Vector pos, Matrix rot);

void SendResetMarketOverride(uint client);

// Conn hash
uint connSystemID = CreateID("LI06");

// A map of all docking requests pending approval by the carrier
map<uint, uint> mapPendingDockingRequests;

// Is debug mode running
static int set_iPluginDebug = 1;

// The distance to undock from the carrier
static int set_iMobileDockOffset = 100;

PLUGIN_RETURNCODE returncode;

extern map<uint, CLIENT_DATA> clients;