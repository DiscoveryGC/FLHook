#pragma once

#include <windows.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <math.h>
#include <list>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <FLHook.h>
#include <plugin.h>
#include <PluginUtilities.h>

enum ALLOWDOCKMODE
{
	ALLOW_ALL = 0,
	ALLOW_GROUP = 1,
	ALLOW_NONE = 2
};

struct CLIENT_DATA
{
	uint iDockingModulesInstalled = 0;
	int iDockingModulesAvailable = 0;
	ALLOWDOCKMODE dockMode = ALLOW_GROUP;
};

struct DELAYEDDOCK
{
	uint carrierID;
	uint dockingID;
	uint timeLeft;
	Vector startPosition;
};

struct CARRIERINFO
{
	vector<wstring> dockedShipList;
	time_t lastCarrierLogin;
};

struct DOCKEDCRAFTINFO
{
	wstring carrierName;
	uint lastDockedSolar;
};

void SendResetMarketOverride(uint client);
void SendSetBaseInfoText2(UINT client, const wstring &message);
void ForceLaunch(uint client);

// The distance to undock from the carrier
static float set_iMobileDockOffset = 100.0f;

// A map of all docking requests pending approval by the carrier
// key = docking client id, value = carrier client id
extern unordered_map<uint, uint> mapPendingDockingRequests;
