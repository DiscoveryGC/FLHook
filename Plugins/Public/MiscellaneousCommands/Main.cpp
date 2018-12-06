// FLHook Plugin to hold a miscellaneous collection of commands and 
// other such things that don't fit into other plugins
//
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Includes
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Main.h"

// A return code to indicate to FLHook if we want the hook processing to continue.
PLUGIN_RETURNCODE returncode;

void LoadSettings();

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	srand(static_cast<uint>(time(nullptr)));
	// If we're being loaded from the command line while FLHook is running then
	// set_scCfgFile will not be empty so load the settings as FLHook only
	// calls load settings on FLHook startup and .rehash.
	if(fdwReason == DLL_PROCESS_ATTACH)
	{
		if (set_scCfgFile.length()>0)
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Structures and Declarations
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

// See Main.h for any struct/class defs.
// This is just for declarations


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Loading Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;
	// Reserved for future use
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Command Functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

// /refresh - Updates the timestamps of the character file for all the ships on the account.
bool UserCmd_RefreshAccounts(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	uint iBase;
	pub::Player::GetBase(iClientID, iBase);

	if (!iBase)
	{
		PrintUserCmdText(iClientID, L"You must be docked to use this command.");
		return true;
	}

	FILETIME ft;
	SYSTEMTIME st;
	GetLocalTime(&st);
	SystemTimeToFileTime(&st, &ft);
	char toWrite[128];
	sprintf_s(toWrite, "%u,%u", ft.dwHighDateTime, ft.dwLowDateTime);
	
	CAccount *acc = HkGetAccountByClientID(iClientID);
	CAccountListNode *accountList = acc->pFirstListNode;
	PrintUserCmdText(iClientID, L"Refreshing Accounts:");
	PrintUserCmdText(iClientID, L""); // Line break to make it look nice
	int iAccountsRefreshed = 0; // Number to keep track of
	HK_ERROR err; // If it errors we want to inform them

	// Save the current char in case for whatever reason they haven't been saved already
	// Prevents any potential data loss.
	HkSaveChar(reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID)));

	// We need to keep track of the first character name lest it loops forever.
	wstring wscFirstCharacterName;
	while (accountList)
	{
		// We need to make sure the character name is not null (which is is sometimes because FLHook?)
		if (!accountList->wszCharname)
		{
			// Loop to the next one if this happens.
			accountList = accountList->next;
			continue;
		}

		// The current name wont cause an exception, so lets use it
		const wstring wscCharacterName = reinterpret_cast<const wchar_t*>(accountList->wszCharname);

		// Only store the first name
		if (iAccountsRefreshed == 0)
			wscFirstCharacterName = wscCharacterName;

		// If this isn't the first name, we need to check that we've not looped over the list again
		else
			if (wscCharacterName == wscFirstCharacterName)
				break; // End the loop if the names match

		iAccountsRefreshed++;
		PrintUserCmdText(iClientID, L"Character: %s - Refreshed for 6 months.", wscCharacterName.c_str());
		if ((err = HkFLIniWrite(wscCharacterName, L"tstamp", stows(toWrite))) != HKE_OK)
		{
			PrintUserCmdText(iClientID, L"ERROR: %s", HkErrGetText(err).c_str());
			return true;
		}

		// Loop again
		accountList = accountList->next;
	}
	// List the amount of accounts we've refreshed.
	PrintUserCmdText(iClientID, L"Sucessfully refreshed %u accounts.", iAccountsRefreshed); 

	// Kick them as a way to prevent save data loss and ensure everything ticks over as intended.
	HkKickReason(reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID)), L"Updating Character File, please wait 10 seconds before reconnecting.");
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Actual Code
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Clean up when a client disconnects */
void ClearClientInfo(uint iClientID)
{
	returncode = DEFAULT_RETURNCODE;
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
	{ L"/refresh", UserCmd_RefreshAccounts, L"" },
	{ L"/refresh*", UserCmd_RefreshAccounts, L"" },
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
	p_PI->sName = "Miscellaneous Commands by a lot of different people.";
	p_PI->sShortName = "misc";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;
	
	p_PI->lstHooks.emplace_back(reinterpret_cast<FARPROC*>(&LoadSettings), PLUGIN_LoadSettings, 0);
	p_PI->lstHooks.emplace_back(reinterpret_cast<FARPROC*>(&ClearClientInfo), PLUGIN_ClearClientInfo, 0);
	p_PI->lstHooks.emplace_back(reinterpret_cast<FARPROC*>(&UserCmd_Process), PLUGIN_UserCmd_Process, 0);

	return p_PI;
}
