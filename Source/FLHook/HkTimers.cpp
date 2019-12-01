#include <winsock2.h>
#include "wildcards.hh"
#include "hook.h"
#include <math.h>

CTimer::CTimer(string sFunc, uint iWarn)
{
	iMax = 0;
	iWarning = iWarn;
	sFunction = sFunc;
}

void CTimer::start()
{
	tmStart = timeInMS();
}

uint CTimer::stop()
{
	uint iDelta = abs((int)(timeInMS() - tmStart));

	if (iDelta > iMax && iDelta > iWarning) {

		// log
		if (set_bPerfTimer)
			HkAddPerfTimerLog("Spent %d ms in %s, longest so far.", iDelta, sFunction.c_str());
		iMax = iDelta;
	}
	else if (iDelta > set_iTimerDebugThreshold && set_iTimerDebugThreshold > 0)
	{
		if (set_bPerfTimer)
			HkAddPerfTimerLog("Spent %d ms in %s", iDelta, sFunction.c_str());
	}

	return iDelta;

}

/**************************************************************************************************************
check if players should be kicked
**************************************************************************************************************/

void HkTimerCheckKick()
{

	CALL_PLUGINS_V(PLUGIN_HkTimerCheckKick, , (), ());


	try {
		// for all players
		struct PlayerData *pPD = 0;
		while (pPD = Players.traverse_active(pPD))
		{
			uint iClientID = HkGetClientIdFromPD(pPD);
			if (iClientID < 1 || iClientID > MAX_CLIENT_ID)
				continue;

			if (ClientInfo[iClientID].tmKickTime)
			{
				if (timeInMS() >= ClientInfo[iClientID].tmKickTime)
				{
					HkKick(ARG_CLIENTID(iClientID)); // kick time expired
					ClientInfo[iClientID].tmKickTime = 0;
				}
				continue; // player will be kicked anyway
			}

			if (set_iAntiBaseIdle)
			{ // anti base-idle check
				uint iBaseID;
				pub::Player::GetBase(iClientID, iBaseID);
				if (iBaseID && ClientInfo[iClientID].iBaseEnterTime)
				{
					if ((time(0) - ClientInfo[iClientID].iBaseEnterTime) >= set_iAntiBaseIdle)
					{
						HkAddKickLog(iClientID, L"Base idling");
						HkMsgAndKick(iClientID, L"Base idling", set_iKickMsgPeriod);
						ClientInfo[iClientID].iBaseEnterTime = 0;
					}
				}
			}

			if (set_iAntiCharMenuIdle)
			{ // anti charmenu-idle check
				if (HkIsInCharSelectMenu(iClientID)) {
					if (!ClientInfo[iClientID].iCharMenuEnterTime)
						ClientInfo[iClientID].iCharMenuEnterTime = (uint)time(0);
					else if ((time(0) - ClientInfo[iClientID].iCharMenuEnterTime) >= set_iAntiCharMenuIdle) {
						HkAddKickLog(iClientID, L"Charmenu idling");
						HkKick(ARG_CLIENTID(iClientID));
						ClientInfo[iClientID].iCharMenuEnterTime = 0;
						continue;
					}
				}
				else
					ClientInfo[iClientID].iCharMenuEnterTime = 0;
			}

		}
	}
	catch (...) { LOG_EXCEPTION }
}

/**************************************************************************************************************
Check if NPC spawns should be disabled
**************************************************************************************************************/

void HkTimerNPCAndF1Check()
{
	CALL_PLUGINS_V(PLUGIN_HkTimerNPCAndF1Check, , (), ());

	try {
		struct PlayerData *pPD = 0;
		while (pPD = Players.traverse_active(pPD))
		{
			uint iClientID = HkGetClientIdFromPD(pPD);
			if (iClientID < 1 || iClientID > MAX_CLIENT_ID)
				continue;

			if (ClientInfo[iClientID].tmF1Time && (timeInMS() >= ClientInfo[iClientID].tmF1Time)) { // f1
				Server.CharacterInfoReq(iClientID, false);
				ClientInfo[iClientID].tmF1Time = 0;
			}
			else if (ClientInfo[iClientID].tmF1TimeDisconnect && (timeInMS() >= ClientInfo[iClientID].tmF1TimeDisconnect)) {
				ulong lArray[64] = { 0 };
				lArray[26] = iClientID;
				__asm
				{
					pushad
					lea ecx, lArray
					mov eax, [hModRemoteClient]
					add eax, ADDR_RC_DISCONNECT
					call eax; disconncet
					popad
				}

				ClientInfo[iClientID].tmF1TimeDisconnect = 0;
				continue;
			}
		}

		// npc
		if (set_iDisableNPCSpawns && (g_iServerLoad >= set_iDisableNPCSpawns))
			HkChangeNPCSpawn(true); // serverload too high, disable npcs
		else
			HkChangeNPCSpawn(false);
	}
	catch (...) { LOG_EXCEPTION }
}
