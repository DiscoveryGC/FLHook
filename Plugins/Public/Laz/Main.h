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

//std::map<uint, uint> PreviousSystemID;

using namespace std;

namespace Help
{
	void LoadSettings(const string &scPluginCfgFile);
	bool UserCmd_HelpStart(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage);
	bool UserCmd_HelpRP(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage);
	bool UserCmd_HelpMine(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage);
	bool UserCmd_HelpCombat(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage);
	bool UserCmd_HelpCash(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage);
	bool UserCmd_HelpPower(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage);
	bool UserCmd_HelpTrade(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage);
	bool UserCmd_HelpAll(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage);
	bool UserCmd_HelpChat(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage);
	bool UserCmd_HelpRep(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage);
	bool textBoxPopUp(uint iClientID, const wstring &wXML, const wstring &wscTitle);
}

namespace Laws
{
	void LoadSettings(const string &scPluginCfgFile);
	void PlayerLaunch(uint iShip, unsigned int iClientID);
	void JumpInComplete(unsigned int system, unsigned int ship);
	//static void CheckSystem(int iClientID);
	static void CheckSector(int iClientID);
	bool UserCmd_Laws(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage);
	static void CheckCargo(int iClientID);
	void ContrabandWarning(uint iClientID, wstring wscText, ...);
}
#endif