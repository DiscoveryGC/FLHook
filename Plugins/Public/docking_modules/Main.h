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

// Structs for timers.

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
extern vector<uint> boostedAmmo;

extern DB::ID_TRAITS defaultTraits;

extern uint ID_lootcrate;
extern uint ID_nanobots;
extern uint ID_batteries;
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


// Utilities.cpp
void SendCommand(uint client, const wstring &message);
void SendSetBaseInfoText2(UINT client, const wstring &message);
void SendResetMarketOverride(UINT client);
uint GetProxyBaseForCarrier(uint carrierClientID);
uint GetProxyBaseForSystem(uint carrierClientID, uint iSystemID);
wstring HkGetCharnameFromCharFile(string charFile, CAccount* acc);
string DecodeWStringToStringOfBytes(wstring& wstr);


// ModuleWatcher.cpp
wstring EnumerateDockedShips(uint carrierClientID);

namespace ModuleWatcher
{
	void __stdcall CharacterSelect_AFTER(struct CHARACTER_ID const & cId, unsigned int iClientID);
	void __stdcall ReqEquipment_AFTER(class EquipDescList const &edl, unsigned int iClientID);
	void __stdcall ReqAddItem_AFTER(unsigned int p1, char const *p2, int p3, float p4, bool p5, unsigned int iClientID);
	void __stdcall ReqRemoveItem(unsigned short p1, int p2, unsigned int iClientID);
	void __stdcall SPScanCargo_AFTER(unsigned int const &p1, unsigned int const &p2, unsigned int iClientID);
}


// PlayerCommands.cpp
ErrorMessage TryDockAtBase(vector<MODULE_CACHE> &Modules, uint dockingClientID, uint carrierClientID, wstring dockingName);
ErrorMessage TryDockInSpace(vector<MODULE_CACHE> &Modules, uint dockingClientID, uint carrierClientID, uint dockingShip, uint carrierShip, wstring dockingName);
void Jettison(vector<MODULE_CACHE>::iterator it, uint dockedClientID, uint carrierClientID);
void CancelRequest(uint dockingClientID);
void SwitchSystem(uint iClientID, uint iShip);
int CheckIfResupplyingAvailable(uint carrierClientID, uint dockedClientID, uint moduleArch, SUPPLIES_INFO& info, bool notify);

namespace Commands
{
	bool Listdocked(uint iClientID, const wstring& wscCmd);
	bool Conn(uint iClientID, const wstring& wscCmd);
	bool Return(uint iClientID, const wstring& wscCmd);
	bool Renameme(uint iClientID, const wstring& wscCmd);
	bool Allowdock(uint iClientID, const wstring& wscCmd);
	bool Dockatbase(uint iClientID, const wstring& wscCmd);
	bool Jettisonship(uint iClientID, const wstring& wscCmd);
	bool Jettisonallships(uint iClientID, const wstring& wscCmd);
	bool Loadsupplies(uint iClientID, const wstring& wscCmd);
}


// HkTimers.cpp
void DelayedDocking(uint dockingClientID, uint carrierClientID, uint moduleArch, uint interruptDistance, int delayTimeSeconds);
void DelayedResupply(uint dockedClientID, wstring &dockedCharname, wstring &carrierCharname, RESUPPLY_REQUEST request, int delayTimeSeconds);

namespace Timers
{
	void HkTimerCheckKick();
}