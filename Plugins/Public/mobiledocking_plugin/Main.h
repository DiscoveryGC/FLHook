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
#include <thread>
#include <FLHook.h>
#include <plugin.h>
#include <PluginUtilities.h>

#include <chrono>
#include <mutex>

struct CLIENT_DATA
{
	uint iDockingModulesInstalled = 0;
	vector<wstring> dockedShips = vector<wstring>();

	// The name of the carrier.
	wstring wscDockedWithCharname = L"";

	// The last real base this ship was on.
	uint iLastBaseID = 0;

	// The proxy base this ship is on. 0 if not docked, -1 if undocking, other values if docked.
	uint proxyBaseID = 0;

	Vector carrierPos = Vector { 0, 0, -100000 };
	Matrix carrierRot = Matrix();

	// The system in which current ship is. -1 if changing.
	uint carrierSystem = 0;
};

struct SHIP_LOCATION
{
	uint baseID = 0;
	uint systemID = 0;
	Vector pos = Vector { 0, 0, -100000 };
	Vector rot = Vector { 0, 0, 0 };
};

typedef bool(*_UserCmdProc)(uint, const wstring &);

struct USERCMD
{
	wchar_t *wszCmd;
	_UserCmdProc proc;
};

struct AMMO
{
	uint ammoID;
	uint ammoLimit;
	bool stackable = false;
};

struct SUPPLY
{
	int efficiency;
	int type;
};

struct SUPPLY_INFO
{
	bool hasAmmoSup = false;
	bool hasHullSup = false;
	bool hasShieldSup = false;
	bool hasCloakSup = false;
};

struct Task
{
	function<void()> Function;
	chrono::system_clock::time_point expiryTime;

	Task(int timeSeconds, function<void()> task)
	{
		Function = task;
		expiryTime = chrono::system_clock::now() + chrono::seconds(timeSeconds);
	}
};

extern vector<Task> TaskScheduler;

// SaveFiles.cpp
string GetSavePath(wstring& charname, bool isCarrier);
string GetSavePath(string& shipFileName, bool isCarrier);
string GetFLAccPath(wstring& charname); 
void LoadShip(string& shipFileName, uint shipClientId);
void SaveDockInfoCarrier(wstring& charname, uint clientID, const CLIENT_DATA& client);
void SaveDockInfoCarried(wstring& charname, uint clientID, const CLIENT_DATA& client);
SHIP_LOCATION GetCarrierPosOffline(uint dockedClientID, wstring charname = L"");
void JettisonShipOffline(uint carrierClientID, wstring dockedCharname);
void JettisonShipOffline(wstring dockedCharname, wstring carrierCharname);
void UpdateLastBaseOffline(wstring& dockedCharname, uint lastBaseID);
void ThrowCargoOffline(wstring charname, Vector carrierPos, uint carrierSystemID, uint moveToBaseID);
void UpdateDyingCarrierPos(wstring& charname, Vector carrierPos, Matrix carrierOrnt, uint carrierSystem);
bool EditFLFile(vector<string> *linesToDelete, vector<string> *linesToAdd, map<string, vector<string>> *linesToReplace, string& path, bool createNew = false, bool compareHard = false);
bool ReadFLFile(map<string, vector<string>> &fields, string& path);
wstring HkGetCharnameFromCharFile(string charFile, CAccount* acc);

// Main.cpp
void JettisonShip(uint carrierClientID, wstring dockedCharname, bool eraseFromList = true);
void DockShip(uint carrierShip, uint carrierClientID, uint dockingClientID);
int CheckIfResupplyingAvailable(uint carrierClientID, uint dockedClientID, SUPPLY_INFO& info, bool notify);
wstring EnumerateDockedShips(uint carrierClientID);
Vector GetUndockingPosition(Vector carrierPos, Matrix carrierRot);
uint GetProxyBaseForClient(uint clientID);
uint GetProxyBaseForSystem(uint systemID);
uint UpdateAvailableModules(uint clientID);

// ClientCommands.cpp
void SendResetMarketOverride(uint client);
void SendSetBaseInfoText2(UINT client, const wstring &message);

// PlayerCommands.cpp
bool CMD_listdocked(uint client, const wstring& wscCmd);
bool CMD_conn(uint client, const wstring& wscCmd);
bool CMD_return(uint client, const wstring& wscCmd);
bool CMD_renameme(uint client, const wstring& wscCmd);
bool CMD_jettisonship(uint client, const wstring& wscCmd);
bool CMD_jettisonallships(uint client, const wstring& wscCmd);
bool CMD_allowdock(uint client, const wstring& wscCmd);
bool CMD_dockatbase(uint client, const wstring& wscCmd);
bool CMD_loadsupplies(uint client, const wstring& wscCmd);

extern map<uint, CLIENT_DATA> mobiledockClients;

// A map of all docking requests pending approval by the carrier
extern map<uint, uint> mapPendingDockingRequests;
extern uint dockingModuleEquipmentID;

extern int cargoCapacityLimit;
extern int jettisonKickTime;
extern int groupDockDelay;
extern float dockDistance;
extern int undockDistance;
extern int crewMinimum;
extern int crewEfficienyConst;

extern uint crewGoodID;
extern map<uint, AMMO> mapAmmo;
extern map<uint, uint> mapBatteries;
extern uint nanobotsID;
extern uint batteriesID;
extern vector<uint> boostedAmmo;
extern map<uint, SUPPLY> mapSupplies;

#endif