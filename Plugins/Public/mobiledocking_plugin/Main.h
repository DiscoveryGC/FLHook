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
};

struct DELAYEDDOCK
{
	uint carrierID;
	uint dockingID;
	uint timeLeft;
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

// The distance to undock from the carrier
static int set_iMobileDockOffset = 100;

// A map of all docking requests pending approval by the carrier
extern map<uint, uint> mapPendingDockingRequests;
