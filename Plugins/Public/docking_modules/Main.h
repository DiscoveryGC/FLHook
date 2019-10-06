#pragma once

#include "Database.h"
#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <list>
#include <algorithm>
#include <plugin.h>
#include "../playercntl_plugin/Main.h"

using namespace DB;

struct AMMO
{
	uint ammoID;
	uint ammoLimit;
	bool stackable = false;
};

struct SUPPLIES_INFO
{
	bool hasAmmoSup = false;
	bool hasBotsSup = false;
	bool hasShieldSup = false;
	bool hasCloakSup = false;
};

struct RESUPPLY_REQUEST
{
	uint cloak;
	map<uint, int> ammoInCart;
	int cloakBatteriesInCart;
	int nanobotsInCart;
	int batteriesInCart;
};

// Structs for timers

struct ActionDocking
{
	int timeLeft;
	uint dockingClientID;
	uint interruptDistance;
	uint moduleArch;
};

struct ActionResupply
{
	int timeLeft;
	uint dockedClientID;
	wstring carrierCharname;
	wstring dockedCharname;
	RESUPPLY_REQUEST request;
};

struct DEFERREDJUMPS
{
	uint system;
	Vector pos;
	Matrix ornt;
};

struct EQ_ITEM
{
	EQ_ITEM *next;
	uint i2;
	ushort s1;
	ushort sID;
	uint iArchID;
	CacheString hardpoint;
	bool bMounted;
	char sz[3];
	float fStatus;
	uint iCount;
	bool bMission;
};

enum ErrorMessage
{
	OK,
	NO_MODULES,
	TOO_LARGE,
	NO_FREE_MODULES,
	TOO_FAR
};

extern PLUGIN_RETURNCODE returncode;

extern map<uint, AMMO> mapAmmo;
extern map<uint, uint> mapBatteries;
extern map<uint, uint> mapBoostedAmmo;

extern ID_TRAITS defaultTraits;

extern uint ID_object_lootcrate;
extern uint ID_item_nanobots;
extern uint ID_item_batteries;
extern uint ID_sound_accepted;
extern uint ID_sound_canceled;
extern uint ID_sound_docked;
extern uint ID_sound_undocked;
extern uint ID_sound_resupply;

extern uint mapPendingDockingRequests[MAX_CLIENT_ID + 1];
extern uint mapDockingClients[MAX_CLIENT_ID + 1];

extern uint ShipsToLaunch[MAX_CLIENT_ID + 1];
extern uint ForceLandingClients[MAX_CLIENT_ID + 1];
extern uint POBForceLandingClients[MAX_CLIENT_ID + 1];

extern vector<uint> dockingList;
extern vector<ActionDocking> dockingQueues[MAX_CLIENT_ID + 1];
extern vector<ActionResupply> resupplyList;

extern bool JumpingCarriers[MAX_CLIENT_ID + 1];
extern bool JumpingDockedShips[MAX_CLIENT_ID + 1];
extern bool JettisoningClients[MAX_CLIENT_ID + 1];
extern bool ResupplyingClients[MAX_CLIENT_ID + 1];

extern map<uint, DEFERREDJUMPS> mapDeferredJumps;
extern const PBYTE SwitchOut;
extern string dataPath;


// Utilities.cpp
void SendCommand(uint iClientID, const wstring &message);
void SendSetBaseInfoText2(uint iClientID, const wstring &message);
void SendResetMarketOverride(uint iClientID);
uint GetProxyBase(uint carrierClientID, uint iSystemID);
wstring HkGetCharnameFromCharFile(string const &charFile, CAccount *acc);
string DecodeWStringToStringOfBytes(wstring &wstr);


// ModuleWatcher.cpp
wstring EnumerateDockedShips(uint carrierClientID);

namespace ModuleWatcher
{
	void __stdcall CharacterSelect_AFTER(CHARACTER_ID const &cId, uint iClientID);
	void __stdcall ReqEquipment_AFTER(EquipDescList const &edl, uint iClientID);
	void __stdcall ReqAddItem(uint iArchID, char const *cHpName, int iCount, float fHealth, bool bMounted, uint iClientID);
	void __stdcall ReqAddItem_AFTER(uint iArchID, char const *cHpName, int iCount, float fHealth, bool bMounted, uint iClientID);
	void __stdcall ReqRemoveItem(ushort sHpID, int iCount, uint iClientID);
	void __stdcall SPScanCargo_AFTER(uint const &scanningShip, uint const &scannedShip, uint iClientID);
}


// PlayerCommands.cpp
ErrorMessage TryDockAtBase(vector<MODULE_CACHE> &Modules, uint dockingClientID, uint carrierClientID, wstring &dockingName);
ErrorMessage TryDockInSpace(vector<MODULE_CACHE> Modules, uint dockingClientID, uint carrierClientID, uint dockingShip, uint carrierShip);
void Jettison(MODULE_CACHE &module, uint dockedClientID, uint carrierClientID);
void CancelRequest(uint dockingClientID);
void SwitchSystem(uint iClientID, uint iShip);
void CheckIfResupplyingAvailable(uint carrierClientID, uint dockedClientID, uint moduleArch, SUPPLIES_INFO &info);

namespace Commands
{
	bool Listdocked(uint iClientID, const wstring &wscCmd);
	bool Conn(uint iClientID, const wstring &wscCmd);
	bool Return(uint iClientID, const wstring &wscCmd);
	bool Renameme(uint iClientID, const wstring &wscCmd);
	bool Allowdock(uint iClientID, const wstring &wscCmd);
	bool Dockatbase(uint iClientID, const wstring &wscCmd);
	bool Jettisonship(uint iClientID, const wstring &wscCmd);
	bool Jettisonallships(uint iClientID, const wstring &wscCmd);
	bool Loadsupplies(uint iClientID, const wstring &wscCmd);
}


// HkTimers.cpp
void DelayedDocking(uint dockingClientID, uint carrierClientID, uint moduleArch, uint interruptDistance, int delayTimeSeconds);
void DelayedResupply(uint dockedClientID, wstring &dockedCharname, wstring &carrierCharname, RESUPPLY_REQUEST &request, int delayTimeSeconds);

namespace Timers
{
	void HkTimerCheckKick();
}


#define traverse_equipment(iClientID, item) \
	char *szClassPtr; \
	memcpy(&szClassPtr, &Players, 4); \
	szClassPtr += 0x418 * (iClientID - 1); \
	EQ_ITEM *eqLst; \
	memcpy(&eqLst, szClassPtr + 0x27C, 4); \
	item = eqLst->next; \
	while (item != eqLst)

#define continue_traverse(item) \
{ \
	item = item->next; \
	continue; \
}
