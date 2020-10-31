#include "Main.h"

#define PRINT_ERROR() { for(uint i = 0; (i < sizeof(wscError)/sizeof(wstring)); i++) PrintUserCmdText(iClientID, wscError[i]); return; }
#define PRINT_OK() PrintUserCmdText(iClientID, L"OK");
#define PRINT_DISABLED() PrintUserCmdText(iClientID, L"Command disabled");

CONNECTION_DATA ConData[MAX_CLIENT_ID + 1];
bool set_bPingCmd;

uint			set_iPingKickFrame;
uint			set_iPingKick;
uint			set_iFluctKick;
uint			set_iLossKickFrame;
uint			set_iLossKick;
uint			set_iLagDetectionFrame;
uint			set_iLagDetectionMinimum;
uint			set_iLagKick;

// Kick high lag and loss players only if the server load 
// exceeds this threshold.
uint			set_iKickThreshold;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Condata::LoadSettings()
{
	set_iPingKickFrame = IniGetI(set_scCfgFile, "Kick", "PingKickFrame", 30);
	if (!set_iPingKickFrame)
		set_iPingKickFrame = 60;
	set_iPingKick = IniGetI(set_scCfgFile, "Kick", "PingKick", 0);
	set_iFluctKick = IniGetI(set_scCfgFile, "Kick", "FluctKick", 0);
	set_iLossKickFrame = IniGetI(set_scCfgFile, "Kick", "LossKickFrame", 30);
	if (!set_iLossKickFrame)
		set_iLossKickFrame = 60;
	set_iLossKick = IniGetI(set_scCfgFile, "Kick", "LossKick", 0);
	set_iLagDetectionFrame = IniGetI(set_scCfgFile, "Kick", "LagDetectionFrame", 50);
	set_iLagDetectionMinimum = IniGetI(set_scCfgFile, "Kick", "LagDetectionMinimum", 200);
	set_iLagKick = IniGetI(set_scCfgFile, "Kick", "LagKick", 0);
	set_iKickThreshold = IniGetI(set_scCfgFile, "Kick", "KickThreshold", 0);

	set_bPingCmd = IniGetB(set_scCfgFile, "UserCommands", "Ping", false);
}

void ClearConData(uint iClientID)
{
	ConData[iClientID].iAverageLoss = 0;
	ConData[iClientID].iAveragePing = 0;
	ConData[iClientID].iLastLoss = 0;
	ConData[iClientID].iLastPacketsDropped = 0;
	ConData[iClientID].iLastPacketsReceived = 0;
	ConData[iClientID].iLastPacketsSent = 0;
	ConData[iClientID].iPingFluctuation = 0;
	ConData[iClientID].lstLoss.clear();
	ConData[iClientID].lstPing.clear();
	ConData[iClientID].lstObjUpdateIntervalls.clear();
	ConData[iClientID].iLags = 0;
	ConData[iClientID].tmLastObjUpdate = 0;
	ConData[iClientID].tmLastObjTimestamp = 0;

	ConData[iClientID].bException = false;
	ConData[iClientID].sExceptionReason = "";
}

void Condata::ClearClientInfo(uint iClientID)
{
	ClearConData(iClientID);
}

void Condata::UserCmd_Help(uint iClientID, const wstring &wscParam)
{

	if (set_bPingCmd) {
		PrintUserCmdText(iClientID, L"/ping");
		PrintUserCmdText(iClientID, L"/pingtarget");
	}

}

void Condata::HkTimerCheckKick()
{
	if (g_iServerLoad > set_iKickThreshold)
	{
		// for all players
		struct PlayerData *pPD = 0;
		while (pPD = Players.traverse_active(pPD))
		{
			uint iClientID = HkGetClientIdFromPD(pPD);


			if (set_iLossKick)
			{ // check if loss is too high
				if (ConData[iClientID].iAverageLoss > (set_iLossKick))
				{
					ConData[iClientID].lstLoss.clear();
					HkAddKickLog(iClientID, L"High loss");
					HkMsgAndKick(iClientID, L"High loss", set_iKickMsgPeriod);
					// call tempban plugin
					TEMPBAN_BAN_STRUCT tempban;
					tempban.iClientID = iClientID;
					tempban.iDuration = 1; // 1 minute
					Plugin_Communication(TEMPBAN_BAN, &tempban);
				}
			}

			if (set_iPingKick)
			{ // check if ping is too high
				if (ConData[iClientID].iAveragePing > (set_iPingKick))
				{
					ConData[iClientID].lstPing.clear();
					HkAddKickLog(iClientID, L"High ping");
					HkMsgAndKick(iClientID, L"High ping", set_iKickMsgPeriod);
					// call tempban plugin
					TEMPBAN_BAN_STRUCT tempban;
					tempban.iClientID = iClientID;
					tempban.iDuration = 1; // 1 minute
					Plugin_Communication(TEMPBAN_BAN, &tempban);
				}
			}

			if (set_iFluctKick)
			{ // check if ping fluct is too high
				if (ConData[iClientID].iPingFluctuation > (set_iFluctKick))
				{
					ConData[iClientID].lstPing.clear();
					HkAddKickLog(iClientID, L"High fluct");
					HkMsgAndKick(iClientID, L"High ping fluctuation", set_iKickMsgPeriod);
					// call tempban plugin
					TEMPBAN_BAN_STRUCT tempban;
					tempban.iClientID = iClientID;
					tempban.iDuration = 1; // 1 minute
					Plugin_Communication(TEMPBAN_BAN, &tempban);
				}
			}

			if (set_iLagKick)
			{ // check if lag is too high
				if (ConData[iClientID].iLags > (set_iLagKick))
				{
					ConData[iClientID].lstObjUpdateIntervalls.clear();

					HkAddKickLog(iClientID, L"High Lag");
					HkMsgAndKick(iClientID, L"High Lag", set_iKickMsgPeriod);
					// call tempban plugin
					TEMPBAN_BAN_STRUCT tempban;
					tempban.iClientID = iClientID;
					tempban.iDuration = 1; // 1 minute
					Plugin_Communication(TEMPBAN_BAN, &tempban);
				}
			}
		}
	}

	// Are there accounts connected with client IDs greater than max player count?
	// If so, kick them as FLServer is buggy and will use high client IDs but 
	// will not allow character selection on them.
	for (int iClientID = Players.GetMaxPlayerCount() + 1; iClientID <= MAX_CLIENT_ID; iClientID++)
	{
		if (Players[iClientID].iOnlineID)
		{
			CAccount *acc = Players.FindAccountFromClientID(iClientID);
			if (acc)
			{
				//ConPrint(L"Kicking lag bug account iClientID=%u %u\n", iClientID,Players[iClientID].iOnlineID);
				acc->ForceLogout();
				Players.logout(iClientID);
			}
		}
	}
}

/**************************************************************************************************************
Update average ping data
**************************************************************************************************************/

void TimerUpdatePingData()
{

	// for all players
	struct PlayerData *pPD = 0;
	while (pPD = Players.traverse_active(pPD))
	{
		uint iClientID = HkGetClientIdFromPD(pPD);
		if (ClientInfo[iClientID].tmF1TimeDisconnect)
			continue;

		DPN_CONNECTION_INFO ci;
		if (HkGetConnectionStats(iClientID, ci) != HKE_OK)
			continue;

		///////////////////////////////////////////////////////////////
		// update ping data
		if (ConData[iClientID].lstPing.size() >= set_iPingKickFrame)
		{
			// calculate average ping and ping fluctuation
			unsigned int iLastPing = 0;
			ConData[iClientID].iAveragePing = 0;
			ConData[iClientID].iPingFluctuation = 0;
			foreach(ConData[iClientID].lstPing, uint, it) {
				ConData[iClientID].iAveragePing += (*it);
				if (iLastPing != 0) {
					ConData[iClientID].iPingFluctuation += (uint)sqrt((double)pow(((float)(*it) - (float)iLastPing), 2));
				}
				iLastPing = (*it);
			}


			ConData[iClientID].iPingFluctuation /= (uint)ConData[iClientID].lstPing.size();
			ConData[iClientID].iAveragePing /= (uint)ConData[iClientID].lstPing.size();
		}

		// remove old pingdata
		while (ConData[iClientID].lstPing.size() >= set_iPingKickFrame)
			ConData[iClientID].lstPing.pop_back();

		ConData[iClientID].lstPing.push_front(ci.dwRoundTripLatencyMS);
	}
}

/**************************************************************************************************************
Update average loss data
**************************************************************************************************************/

void TimerUpdateLossData()
{

	// for all players 
	float fLossPercentage;
	uint iNewDrops;
	uint iNewSent;
	struct PlayerData *pPD = 0;
	while (pPD = Players.traverse_active(pPD))
	{
		uint iClientID = HkGetClientIdFromPD(pPD);
		if (ClientInfo[iClientID].tmF1TimeDisconnect)
			continue;

		DPN_CONNECTION_INFO ci;
		if (HkGetConnectionStats(iClientID, ci) != HKE_OK)
			continue;

		/////////////////////////////////////////////////////////////// 
		// update loss data 
		if (ConData[iClientID].lstLoss.size() >= (set_iLossKickFrame / (LOSS_INTERVALL / 1000)))
		{
			// calculate average loss 
			ConData[iClientID].iAverageLoss = 0;
			foreach(ConData[iClientID].lstLoss, uint, it)
				ConData[iClientID].iAverageLoss += (*it);

			ConData[iClientID].iAverageLoss /= (uint)ConData[iClientID].lstLoss.size();
		}

		// remove old lossdata 
		while (ConData[iClientID].lstLoss.size() >= (set_iLossKickFrame / (LOSS_INTERVALL / 1000)))
			ConData[iClientID].lstLoss.pop_back();

		//sum of Drops = Drops guaranteed + drops non-guaranteed 
		iNewDrops = (ci.dwPacketsRetried + ci.dwPacketsDropped) - ConData[iClientID].iLastPacketsDropped;

		iNewSent = (ci.dwPacketsSentGuaranteed + ci.dwPacketsSentNonGuaranteed) - ConData[iClientID].iLastPacketsSent;

		// % of Packets Lost = Drops / (sent+received) * 100 
		if (iNewSent > 0) // division by zero check
			fLossPercentage = (float)((float)iNewDrops / (float)iNewSent) * 100;
		else
			fLossPercentage = 0.0;

		if (fLossPercentage > 100)
			fLossPercentage = 100;

		//add last loss to List lstLoss and put current value into iLastLoss 
		ConData[iClientID].lstLoss.push_front(ConData[iClientID].iLastLoss);
		ConData[iClientID].iLastLoss = (uint)fLossPercentage;

		//Fill new ClientInfo-variables with current values 
		ConData[iClientID].iLastPacketsSent = ci.dwPacketsSentGuaranteed + ci.dwPacketsSentNonGuaranteed;
		ConData[iClientID].iLastPacketsDropped = ci.dwPacketsRetried + ci.dwPacketsDropped;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// add timers here
typedef void(*_TimerFunc)();

struct TIMER
{
	_TimerFunc	proc;
	mstime		tmIntervallMS;
	mstime		tmLastCall;
};

TIMER Timers[] =
{
	{ TimerUpdatePingData, 1000, 0 },
	{ TimerUpdateLossData, LOSS_INTERVALL, 0 },
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Condata::Update()
{
	static bool bFirstTime = true;
	if (bFirstTime) {
		bFirstTime = false;
		// check for logged in players and reset their connection data
		struct PlayerData *pPD = 0;
		while (pPD = Players.traverse_active(pPD))
			ClearConData(HkGetClientIdFromPD(pPD));
	}

	// call timers
	for (uint i = 0; (i < sizeof(Timers) / sizeof(TIMER)); i++)
	{
		if ((timeInMS() - Timers[i].tmLastCall) >= Timers[i].tmIntervallMS)
		{
			Timers[i].tmLastCall = timeInMS();
			Timers[i].proc();
		}
	}

	return 0; // it doesnt matter what we return here since we have set the return code to "DEFAULT_RETURNCODE", so FLHook will just ignore it
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Condata::PlayerLaunch(unsigned int iShip, unsigned int iClientID)
{

	ConData[iClientID].tmLastObjUpdate = 0;

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Condata::SPObjUpdate(struct SSPObjUpdateInfo const &ui, unsigned int iClientID)
{
	// lag detection
	IObjInspectImpl *ins = HkGetInspect(iClientID);
	if (!ins)
		return; // ??? 8[

	mstime tmNow = timeInMS();
	mstime tmTimestamp = (mstime)(ui.fTimestamp * 1000);

	if (set_iLagDetectionFrame && ConData[iClientID].tmLastObjUpdate && (HkGetEngineState(iClientID) != ES_TRADELANE) && (ui.cState != 7))
	{
		uint iTimeDiff = (uint)(tmNow - ConData[iClientID].tmLastObjUpdate);
		uint iTimestampDiff = (uint)(tmTimestamp - ConData[iClientID].tmLastObjTimestamp);
		int iDiff = (int)sqrt(pow((long double)((int)iTimeDiff - (int)iTimestampDiff), 2));
		iDiff -= g_iServerLoad;
		if (iDiff < 0)
			iDiff = 0;

		uint iPerc;
		if (iTimestampDiff != 0)
			iPerc = (uint)((float)((float)iDiff / (float)iTimestampDiff)*100.0);
		else
			iPerc = 0;


		if (ConData[iClientID].lstObjUpdateIntervalls.size() >= set_iLagDetectionFrame)
		{
			uint iLags = 0;
			foreach(ConData[iClientID].lstObjUpdateIntervalls, uint, it)
			{
				if ((*it) > set_iLagDetectionMinimum)
					iLags++;
			}

			ConData[iClientID].iLags = (iLags * 100) / set_iLagDetectionFrame;
			while (ConData[iClientID].lstObjUpdateIntervalls.size() >= set_iLagDetectionFrame)
				ConData[iClientID].lstObjUpdateIntervalls.pop_front();
		}

		ConData[iClientID].lstObjUpdateIntervalls.push_back(iPerc);
	}

	ConData[iClientID].tmLastObjUpdate = tmNow;
	ConData[iClientID].tmLastObjTimestamp = tmTimestamp;

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Condata::UserCmd_Ping(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!set_bPingCmd) {
		PRINT_DISABLED();
		return true;
	}

	wstring wscTargetPlayer = GetParam(wscParam, ' ', 0);

	uint iClientIDTarget;
	iClientIDTarget = iClientID;

	wstring Response;

	Response += L"Ping: ";
	if (ConData[iClientIDTarget].lstPing.size() < set_iPingKickFrame)
		Response += L"n/a Fluct: n/a ";
	else {
		Response += stows(itos(ConData[iClientIDTarget].iAveragePing)).c_str();
		Response += L"ms ";
		if (set_iPingKick > 0) {
			Response += L"(Max: ";
			Response += stows(itos(set_iPingKick)).c_str();
			Response += L"ms) ";
		}
		Response += L"Fluct: ";
		Response += stows(itos(ConData[iClientIDTarget].iPingFluctuation)).c_str();
		Response += L"ms ";
		if (set_iFluctKick > 0) {
			Response += L"(Max: ";
			Response += stows(itos(set_iFluctKick)).c_str();
			Response += L"ms) ";
		}
	}

	Response += L"Loss: ";
	if (ConData[iClientIDTarget].lstLoss.size() < (set_iLossKickFrame / (LOSS_INTERVALL / 1000)))
		Response += L"n/a ";
	else {
		Response += stows(itos(ConData[iClientIDTarget].iAverageLoss)).c_str();
		Response += L"%% ";
		if (set_iLossKick > 0) {
			Response += L"(Max: ";
			Response += stows(itos(set_iLossKick)).c_str();
			Response += L"%%) ";
		}
	}

	Response += L"Lag: ";
	if (ConData[iClientIDTarget].lstObjUpdateIntervalls.size() < set_iLagDetectionFrame)
		Response += L"n/a";
	else {
		Response += stows(itos(ConData[iClientIDTarget].iLags)).c_str();
		Response += L"%% ";
		if (set_iLagKick > 0) {
			Response += L"(Max: ";
			Response += stows(itos(set_iLagKick)).c_str();
			Response += L"%%)";
		}
	}

	// Send the message to the user 
	PrintUserCmdText(iClientID, Response);
	return true;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Condata::UserCmd_PingTarget(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!set_bPingCmd) {
		PRINT_DISABLED();
		return true;
	}

	uint iShip = 0;
	pub::Player::GetShip(iClientID, iShip);
	if (!iShip) {
		PrintUserCmdText(iClientID, L"Error: You are docked");
		return true;
	}

	uint iTarget = 0;
	pub::SpaceObj::GetTarget(iShip, iTarget);

	if (!iTarget) {
		PrintUserCmdText(iClientID, L"Error: No target");
		return true;
	}

	uint iClientIDTarget = HkGetClientIDByShip(iTarget);
	if (!HkIsValidClientID(iClientIDTarget))
	{
		PrintUserCmdText(iClientID, L"Error: Target is no player");
		return true;
	}


	wstring Response;

	if (iClientIDTarget != iClientID) {
		wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientIDTarget);
		Response += wscCharname.c_str();
		Response += L" - ";
	}

	Response += L"Ping: ";
	if (ConData[iClientIDTarget].lstPing.size() < set_iPingKickFrame)
		Response += L"n/a Fluct: n/a ";
	else {
		Response += L"[redacted] ";
		if (set_iPingKick > 0) {
			Response += L"(Max: ";
			Response += stows(itos(set_iPingKick)).c_str();
			Response += L"ms) ";
		}
		Response += L"Fluct: ";
		Response += stows(itos(ConData[iClientIDTarget].iPingFluctuation)).c_str();
		Response += L"ms ";
		if (set_iFluctKick > 0) {
			Response += L"(Max: ";
			Response += stows(itos(set_iFluctKick)).c_str();
			Response += L"ms) ";
		}
	}

	Response += L"Loss: ";
	if (ConData[iClientIDTarget].lstLoss.size() < (set_iLossKickFrame / (LOSS_INTERVALL / 1000)))
		Response += L"n/a ";
	else {
		Response += stows(itos(ConData[iClientIDTarget].iAverageLoss)).c_str();
		Response += L"%% ";
		if (set_iLossKick > 0) {
			Response += L"(Max: ";
			Response += stows(itos(set_iLossKick)).c_str();
			Response += L"%%) ";
		}
	}

	Response += L"Lag: ";
	if (ConData[iClientIDTarget].lstObjUpdateIntervalls.size() < set_iLagDetectionFrame)
		Response += L"n/a";
	else {
		Response += stows(itos(ConData[iClientIDTarget].iLags)).c_str();
		Response += L"%% ";
		if (set_iLagKick > 0) {
			Response += L"(Max: ";
			Response += stows(itos(set_iLagKick)).c_str();
			Response += L"%%)";
		}
	}

	// Send the message to the user 
	PrintUserCmdText(iClientID, Response);
	return true;
}