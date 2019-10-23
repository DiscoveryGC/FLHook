// Expanded Help Menu - Customisable Help Commands
// By Laz, w/ help from @Alex.
//
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Includes
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
#include "Main.h"

#include "../hookext_plugin/hookext_exports.h"

/// A return code to indicate to FLHook if we want the hook processing to continue.
PLUGIN_RETURNCODE returncode;

void LoadSettings();

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	srand((uint)time(0));
	// If we're being loaded from the command line while FLHook is running then
	// set_scCfgFile will not be empty so load the settings as FLHook only
	// calls load settings on FLHook startup and .rehash.
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		if (set_scCfgFile.length() > 0)
			LoadSettings();
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
	}
	return true;
}

/// Hook will call this function after calling a plugin function to see if we the
/// processing to continue
EXPORT PLUGIN_RETURNCODE Get_PluginReturnCode()
{
	return returncode;
}

struct HELPSTRUCT
{
	wstring wscTitle; // What will the title of our text box be called?
	wstring wscContent; // What content will it hold?
};

bool bPluginEnabled = true; // So we can disable it with ease.
map<wstring, HELPSTRUCT> mapHelp;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Loading Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LoadSettings()
{
	mapHelp.clear();
	returncode = DEFAULT_RETURNCODE;

	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\laz_help.cfg";
	bPluginEnabled = IniGetB(scPluginCfgFile, "Config", "Enabled", true); // Allow things to be quickly disabled

	HELPSTRUCT helpstruct;
	list<INISECTIONVALUE> iniSection; // Create a new list to store our values
	IniGetSection(scPluginCfgFile, "Commands", iniSection); // Which header will we use to fill the values

	foreach(iniSection, INISECTIONVALUE, iter) // Loop through the list
	{
		if (iter->scKey == "help") // Imitate ini reader
		{
			wstring getValueString = stows(iter->scValue); // Get around the character limit glitch

			int firstComma = getValueString.find(','); // Get index of first comma
			int secondComma = getValueString.substr(firstComma + 1).find(','); // Get index of second comma
			wstring wscParam = getValueString.substr(0, firstComma); // Our param for the command
			helpstruct.wscTitle = Trim(getValueString.substr(firstComma + 1, secondComma)); // The title for our text box
			helpstruct.wscContent = Trim(getValueString.substr(firstComma + secondComma + 2)); // The content of our text box. XML String.

			mapHelp[wscParam] = helpstruct;
		}
	}
	ConPrint(L"HELP MENUS: Loaded %u help menus\n", mapHelp.size());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HelpInfo(uint iClientID)
{
	PrintUserCmdText(iClientID, L"Below is a list of all possible helpmenu commands, but there is also /rules and /start for some basic information for new players.");
	PrintUserCmdText(iClientID, L"The current help commands available are as follows:");
	for (map<wstring, HELPSTRUCT>::iterator iter = mapHelp.begin(); iter != mapHelp.end(); iter++)
	{
		PrintUserCmdText(iClientID, L"helpmenu %s", iter->first.c_str());
	}
}

bool ValidityCheck(uint iClientID)
{
	if (!bPluginEnabled) // Make sure that the plugin isn't disabled via the cfg
	{
		PrintUserCmdText(iClientID, L"The extended help menu is currently disabled.");
		return false;
	}

	// Stops people locking themselves out by using the command when dead.
	if (HkIsOnDeathMenu(iClientID))
	{
		PrintUserCmdText(iClientID, L"ERR: You must be alive to use this command."); // Dummy msg
		return false;
	}
	return true;
}

void TextboxPopUp(uint iClientID, const wstring &wscTitle, const wstring &wscXML)
{

	HkChangeIDSString(iClientID, 500000, wscTitle); // Change Title to second param in the cfg file
	HkChangeIDSString(iClientID, 500001, wscXML); // Change content to xml string in the cfg file

	FmtStr caption(0, 0); // Create new text box title
	caption.begin_mad_lib(500000); // Populate it with the infocard we changed
	caption.end_mad_lib();

	FmtStr message(0, 0); // Create a new message
	message.begin_mad_lib(500001); // Populate that message with the infocard we changed
	message.end_mad_lib();

	pub::Player::PopUpDialog(iClientID, caption, message, POPUPDIALOG_BUTTONS_CENTER_OK);
	// Display that message in game

	PrintUserCmdText(iClientID, L"OK");
}

// We need a seperate one for special infocards, ones that are not controlled by this plugin
// In this case, there have already been infocards for "Getting Started", and "Server Rules" created,
// So we'll use those and skip out on any potential bugs with HkChangeIDSString
void RulesOrStarted(uint iClientID, const wstring &wscTitle, const int infocardNumber)
{
	HkChangeIDSString(iClientID, 500000, wscTitle);

	FmtStr caption(0, 0);
	caption.begin_mad_lib(500000);
	caption.end_mad_lib();

	FmtStr message(0, 0);
	message.begin_mad_lib(infocardNumber);
	message.end_mad_lib();

	pub::Player::PopUpDialog(iClientID, caption, message, POPUPDIALOG_BUTTONS_CENTER_OK);

	PrintUserCmdText(iClientID, L"OK");
}

bool UserCmd_Rules(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!ValidityCheck(iClientID))
		return false;

	RulesOrStarted(iClientID, L"Discovery RP 24/7 Server Rules", 501446);
	return true;
}

bool UserCmd_GettingStarted(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!ValidityCheck(iClientID))
		return false;

	RulesOrStarted(iClientID, L"Getting Started", 500709);
	return true;
}

bool UserCmd_HelpMenu(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!ValidityCheck(iClientID)) // See function for details
		return false;

	wstring wscHelpParam = ToLower(GetParam(wscParam, ' ', 0)); // What help list are we trying to get?

	if (!wscHelpParam.length()) // If no param was provided, iterate through the list.
	{
		HelpInfo(iClientID);
		return false;
	}

	if (mapHelp.find(wscHelpParam) != mapHelp.end())
	{
		HELPSTRUCT &xml = mapHelp[wscHelpParam]; // Load in the correct struct
		wstring wscContent = L"<RDL><PUSH/>" + xml.wscContent + L"<PARA/><POP/></RDL>"; // Format it to valid XML
		TextboxPopUp(iClientID, xml.wscTitle, wscContent); // Send it to be turned into a message box
		return true;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Client command processing
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef bool(*_UserCmdProc)(uint, const wstring &, const wstring &, const wchar_t*);

struct USERCMD
{
	wchar_t *wszCmd;
	_UserCmdProc proc;
	wchar_t *usage;
};

USERCMD UserCmds[] =
{
	{ L"/helpmenu", UserCmd_HelpMenu, L"Usage: /helpmenu" },
	{ L"/helpmenu*", UserCmd_HelpMenu, L"Usage: /helpmenu" },
	{ L"/start", UserCmd_GettingStarted, L"Usage: /start" },
	{ L"/start*", UserCmd_GettingStarted, L"Usage: /start" },
	{ L"/rules", UserCmd_Rules, L"Usage: /rules" },
	{ L"/rules*", UserCmd_Rules, L"Usage: /rules" },
};

/**
This function is called by FLHook when a user types a chat string. We look at the
string they've typed and see if it starts with one of the above commands. If it
does we try to process it.
*/
bool UserCmd_Process(uint iClientID, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	wstring wscCmdLineLower = ToLower(wscCmd);

	// If the chat string does not match the USER_CMD then we do not handle the
	// command, so let other plugins or FLHook kick in. We require an exact match
	for (uint i = 0; (i < sizeof(UserCmds) / sizeof(USERCMD)); i++)
	{

		if (wscCmdLineLower.find(UserCmds[i].wszCmd) == 0)
		{
			// Extract the parameters string from the chat string. It should
			// be immediately after the command and a space.
			wstring wscParam = L"";
			if (wscCmd.length() > wcslen(UserCmds[i].wszCmd))
			{
				if (wscCmd[wcslen(UserCmds[i].wszCmd)] != ' ')
					continue;
				wscParam = wscCmd.substr(wcslen(UserCmds[i].wszCmd) + 1);
			}

			// Dispatch the command to the appropriate processing function.
			if (UserCmds[i].proc(iClientID, wscCmd, wscParam, UserCmds[i].usage))
			{
				// We handled the command tell FL hook to stop processing this
				// chat string.
				returncode = SKIPPLUGINS_NOFUNCTIONCALL; // we handled the command, return immediatly
				return true;
			}
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions to hook
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "Expanded Help Menu by Laz";
	p_PI->sShortName = "exhelp";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));

	return p_PI;
}
