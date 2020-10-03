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
	Universe::IBase *undockNPCBase = 0;
	uint undockPoBID = 0;

	// A flag denoting that the above base should be used as an undock point
	bool baseUndock = false;

	// This shows you if the character is on died carrier right now.
	bool carrierDied = false;

	//Carrier related variable
	bool IsCarrierInBase = false;

	//Snub related variable
	bool DockedSomeBasesWhileInCarrier = false;
	bool IsInsideOfflineCarrierInBase = false;
	bool isCargoEmpty = true;
	bool Disconnected = false;

	wstring charfilename = L"";
	uint clientID = 0;
};

struct ActionJettison
{
	int timeLeft;
	wstring carrierCharname;
	wstring dockedCharname;
};

extern vector<ActionJettison> jettisonList;

void SendCommand(uint client, const wstring &message);
void SendResetMarketOverride(uint client);
void SendSetBaseInfoText2(UINT client, const wstring &message);
void ForceLaunch(uint client);

// The distance to undock from the carrier
static int set_iMobileDockOffset = 100;

extern map<wstring, CLIENT_DATA> mobiledockClients;

// A map of all docking requests pending approval by the carrier
extern map<uint, uint> mapPendingDockingRequests;
struct InviteLink
{
	uint iBase;
	wstring Carrier;
	uint Password;
	uint Time;
	const wchar_t* carrierwchar;
	wstring CarrierWCharName;
};
extern map<uint, list<InviteLink>> mapInviteLinks;
