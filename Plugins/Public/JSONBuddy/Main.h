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
#include <FLHook.h>
#include <plugin.h>
#include <PluginUtilities.h>

namespace Condata
{
	void LoadSettings();
	void ClearClientInfo(uint iClientID);
	void UserCmd_Help(uint iClientID, const wstring &wscParam);
	void HkTimerCheckKick();
	void SPObjUpdate(struct SSPObjUpdateInfo const &ui, unsigned int iClientID);
	int Update();
	void PlayerLaunch(unsigned int iShip, unsigned int iClientID);
	bool UserCmd_Ping(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
	bool UserCmd_PingTarget(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
}

///////////////////////////
// Condata Definitions   //
///////////////////////////
#define LOSS_INTERVALL 4000

struct CONNECTION_DATA
{
	// connection data	
	list<uint>	lstLoss;
	uint		iLastLoss;
	uint		iAverageLoss;
	list<uint>	lstPing;
	uint		iAveragePing;
	uint		iPingFluctuation;
	uint		iLastPacketsSent;
	uint		iLastPacketsReceived;
	uint		iLastPacketsDropped;
	uint		iLags;
	vector<uint>	lstObjUpdateIntervalls;
	mstime		tmLastObjUpdate;
	mstime		tmLastObjTimestamp;
	IObjInspectImpl* obj;


	// exception
	bool		bException;
	string		sExceptionReason;
};

extern CONNECTION_DATA ConData[250];

extern uint			set_iPingKickFrame;
extern uint			set_iPingKick;
extern uint			set_iFluctKick;
extern uint			set_iLossKickFrame;
extern uint			set_iLossKick;
extern uint			set_iLagDetectionFrame;
extern uint			set_iLagDetectionMinimum;
extern uint			set_iLagKick;

// Kick high lag and loss players only if the server load 
// exceeds this threshold.
extern uint			set_iKickThreshold;

///////////////////////////
// Condata Definitions   //
///////////////////////////

struct ACTIVITY_DATA
{
	string charname;
	string ip;
	string shiparch;
	string id;
};

extern map <uint, ACTIVITY_DATA> mapActivityData;

using namespace std;

#endif
