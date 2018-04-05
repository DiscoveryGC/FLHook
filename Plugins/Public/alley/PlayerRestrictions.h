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



//bool UserCmd_ShowRestrictions(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
void SetFuse(uint iClientID, uint fuse, float lifetime);
void UnSetFuse(uint iClientID, uint fuse);

using namespace std;
/*
namespace ANPC
{
	void LoadSettings();
	bool CreateNPC(int AAmount, uint AShiparch, uint ALoadout, const char *AIFF, uint iShip1, uint iSystem);
	void AdminCmd_ListNPCFleets(CCmds* cmds);
	void AdminCmd_AIFleet(CCmds* cmds, wstring FleetName);
	void AdminCmd_AIMake(CCmds* cmds, int Amount, wstring NpcType);
	void AdminCmd_AILoot(CCmds* cmds, int Toggle);
	void AIDestroy();
	void AdminCmd_AIKill(CCmds* cmds, int loot);
	void AdminCmd_AIEvade(CCmds* cmds);
	void AdminCmd_AICancel(CCmds* cmds);
	void AdminCmd_AICome(CCmds* cmds);
	void AdminCmd_AIFormation(CCmds* cmds);
	void AdminCmd_AIFollow(CCmds* cmds);
	void AdminCmd_AIRep(CCmds* cmds);
	bool IsFLHook(CShip* ship);
}
*/

namespace SCI
{
	void LoadSettings();
	void CheckItems(unsigned int iClientID);
	void ClearClientInfo(unsigned int iClientID);
	void CheckOwned(unsigned int iClientID);
	void UpdatePlayerID(unsigned int iClientID);
	bool CanDock(uint iDockTarget, uint iClientID);
}

namespace REP
{
	void LoadSettings();
}

namespace ADOCK
{
	void LoadSettings();
	void ClearClientInfo(unsigned int iClientID);
	void PlayerLaunch(unsigned int iShip, unsigned int client);
	bool NoDockCommand(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
	void Timer();
	bool IsDockAllowed(uint iShip, uint iDockTarget, uint iClientID);
	void AdminNoDock(CCmds* cmds, const wstring &wscCharname);
	bool PoliceCmd(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
}


namespace AP
{
	void LoadSettings();
	bool AlleyCmd_Help(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
	bool AlleyCmd_Chase(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
	bool RacestartCmd(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage);
	bool SystemSwitchOutComplete(unsigned int iShip, unsigned int iClientID);
	void ClearClientInfo(unsigned int iClientID);
	void BaseEnter_AFTER(uint iBaseID, uint iClientID);
	void Timer();
}

#endif
