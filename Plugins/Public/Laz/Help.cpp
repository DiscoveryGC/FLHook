// Player Control plugin for FLHookPlugin
// Feb 2010 by Cannon
//
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

#include <windows.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <io.h>
#include <string>
#include <time.h>
#include <FLHook.h>
#include <plugin.h>
#include <FLCoreServer.h>
#include <FLCoreCommon.h>
#include <PluginUtilities.h>
#include <math.h>
#include <map>
#include <list>
#include <vector>
#include "Main.h"

/// Load the configuration
void LoadSettingsUtl()
{
	// The path to the configuration file.
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scHelpPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\laz_help.cfg";
}

namespace Help
{
	static bool set_helpEnabled;
	string helpStart;
	string helpChat;
	string helpRP;
	string helpTrade;
	string helpMine;
	string helpCash;
	string helpCombat;
	string helpPower;
	string helpRep;
	string helpAll;

	//This function is called when the admin command rehash is called and when the module is loaded.

	void Help::LoadSettings(const string &scPluginCfgFile)
	{
		// Search for Plugin Configs
		set_helpEnabled = IniGetB(scPluginCfgFile, "Config", "Enabled", true);
		helpStart = IniGetS(scPluginCfgFile, "Config", "Start", "ERR NO CONFIG FOUND");
		helpChat = IniGetS(scPluginCfgFile, "Config", "Chat", "ERR NO CONFIG FOUND");
		helpRP = IniGetS(scPluginCfgFile, "Config", "RP", "ERR NO CONFIG FOUND");
		helpTrade = IniGetS(scPluginCfgFile, "Config", "Trade", "ERR NO CONFIG FOUND");
		helpMine = IniGetS(scPluginCfgFile, "Config", "Mine", "ERR NO CONFIG FOUND");
		helpCombat = IniGetS(scPluginCfgFile, "Config", "Combat", "ERR NO CONFIG FOUND");
		helpPower = IniGetS(scPluginCfgFile, "Config", "Power", "ERR NO CONFIG FOUND");
		helpRep = IniGetS(scPluginCfgFile, "Config", "Rep", "ERR NO CONFIG FOUND");
		helpCash = IniGetS(scPluginCfgFile, "Config", "Cash", "ERR NO CONFIG FOUND");
		helpAll = IniGetS(scPluginCfgFile, "Config", "All", "ERR NO CONFIG FOUND");
		// Last value is what shall be shown if there are no other values provided in the cfg file.
	}

	bool Help::textBoxPopUp(uint iClientID, const wstring &wXML, const wstring &wscTitle)
	{
		struct PlayerData *pPD = 0;
		while (pPD = Players.traverse_active(pPD))
		{
			uint iClientID = HkGetClientIdFromPD(pPD);

			HkChangeIDSString(iClientID, 500000, wXML);
			HkChangeIDSString(iClientID, 2, wscTitle);

			FmtStr caption(0, 0);
			caption.begin_mad_lib(2);
			caption.end_mad_lib();

			FmtStr message(0, 0);
			message.begin_mad_lib(500000);
			message.end_mad_lib();

			pub::Player::PopUpDialog(iClientID, caption, message, POPUPDIALOG_BUTTONS_CENTER_OK);

			PrintUserCmdText(iClientID, L"OK");
			return true;
		}
		return false;
	}

	bool Help::UserCmd_HelpStart(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage)
	{
		//If [Config]
		//Enabled = yes
		//Run code
		if (set_helpEnabled)
		{
			wstring XML = stows(helpStart);
			wstring wscBoxContents = L"<RDL><PUSH/>" + XML + L"<PARA/><POP/></RDL>";
			wstring wscTitle = L"Welcome to Discovery!";
			Help::textBoxPopUp(iClientID, wscBoxContents, wscTitle);
		}
		return true;
	}
	bool Help::UserCmd_HelpChat(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage)
	{
		if (set_helpEnabled)
		{
			wstring XML = stows(helpChat);
			wstring wscBoxContents = L"<RDL><PUSH/>" + XML + L"<PARA/><POP/></RDL>";
			wstring wscTitle = L"Chat Commands";
			Help::textBoxPopUp(iClientID, wscBoxContents, wscTitle);
		}
		return true;
	}
	bool Help::UserCmd_HelpMine(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage)
	{
		if (set_helpEnabled)
		{
			wstring XML = stows(helpMine);
			wstring wscBoxContents = L"<RDL><PUSH/>" + XML + L"<PARA/><POP/></RDL>";
			wstring wscTitle = L"How To Get Started Mining!";
			Help::textBoxPopUp(iClientID, wscBoxContents, wscTitle);
		}
		return true;
	}
	bool Help::UserCmd_HelpRP(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage)
	{
		if (set_helpEnabled)
		{
			wstring XML = stows(helpRP);
			wstring wscBoxContents = L"<RDL><PUSH/>" + XML + L"<PARA/><POP/></RDL>";
			wstring wscTitle = L"Roleplaying and You!";
			Help::textBoxPopUp(iClientID, wscBoxContents, wscTitle);
		}
		return true;
	}
	bool Help::UserCmd_HelpTrade(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage)
	{
		if (set_helpEnabled)
		{
			wstring XML = stows(helpTrade);
			wstring wscBoxContents = L"<RDL><PUSH/>" + XML + L"<PARA/><POP/></RDL>";
			wstring wscTitle = L"Trading 101";
			Help::textBoxPopUp(iClientID, wscBoxContents, wscTitle);
		}
		return true;
	}
	bool Help::UserCmd_HelpCash(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage)
	{
		if (set_helpEnabled)
		{
			wstring XML = stows(helpCash);
			wstring wscBoxContents = L"<RDL><PUSH/>" + XML + L"<PARA/><POP/></RDL>";
			wstring wscTitle = L"Cash 'n Credits";
			Help::textBoxPopUp(iClientID, wscBoxContents, wscTitle);
		}
		return true;
	}
	bool Help::UserCmd_HelpRep(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage)
	{
		if (set_helpEnabled)
		{
			wstring XML = stows(helpRep);
			wstring wscBoxContents = L"<RDL><PUSH/>" + XML + L"<PARA/><POP/></RDL>";
			wstring wscTitle = L"Reputation Information";
			Help::textBoxPopUp(iClientID, wscBoxContents, wscTitle);
		}
		return true;
	}
	bool Help::UserCmd_HelpPower(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage)
	{
		if (set_helpEnabled)
		{
			wstring XML = stows(helpPower);
			wstring wscBoxContents = L"<RDL><PUSH/>" + XML + L"<PARA/><POP/></RDL>";
			wstring wscTitle = L"Powercore Problems?";
			Help::textBoxPopUp(iClientID, wscBoxContents, wscTitle);
		}
		return true;
	}
	bool Help::UserCmd_HelpCombat(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage)
	{
		if (set_helpEnabled)
		{
			wstring XML = stows(helpCombat);
			wstring wscBoxContents = L"<RDL><PUSH/>" + XML + L"<PARA/><POP/></RDL>";
			wstring wscTitle = L"How to get gud.";
			Help::textBoxPopUp(iClientID, wscBoxContents, wscTitle);
		}
		return true;
	}
	bool Help::UserCmd_HelpAll(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage)
	{
		if (set_helpEnabled)
		{
			wstring XML = stows(helpAll);
			wstring wscBoxContents = L"<RDL><PUSH/>" + XML + L"<PARA/><POP/></RDL>";
			wstring wscTitle = L"Everything you need!";
			Help::textBoxPopUp(iClientID, wscBoxContents, wscTitle);
		}
		return true;
	}
}
