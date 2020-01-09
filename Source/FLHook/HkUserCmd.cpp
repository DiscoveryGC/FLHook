#include "hook.h"

#define PRINT_ERROR() { for(uint i = 0; (i < sizeof(wscError)/sizeof(wstring)); i++) PrintUserCmdText(iClientID, L"%s", wscError[i].c_str()); return; }
#define PRINT_OK() PrintUserCmdText(iClientID, L"OK");
#define PRINT_DISABLED() PrintUserCmdText(iClientID, L"Command disabled");
#define GET_USERFILE(a) string a; { CAccount *acc = Players.FindAccountFromClientID(iClientID); wstring wscDir; HkGetAccountDirName(acc, wscDir); a = scAcctPath + wstos(wscDir) + "\\flhookuser.ini"; }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef void(*_UserCmdProc)(uint, const wstring &);

struct USERCMD
{
	wchar_t *wszCmd;
	_UserCmdProc proc;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PrintUserCmdText(uint iClientID, wstring wscText, ...)
{
	wchar_t wszBuf[1024 * 8] = L"";
	va_list marker;
	va_start(marker, wscText);
	_vsnwprintf(wszBuf, sizeof(wszBuf) - 1, wscText.c_str(), marker);

	wstring wscXML = L"<TRA data=\"" + set_wscUserCmdStyle + L"\" mask=\"-1\"/><TEXT>" + XMLText(wszBuf) + L"</TEXT>";
	HkFMsg(iClientID, wscXML);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UserCmd_SetDieMsg(uint iClientID, const wstring &wscParam)
{
	if (!set_bUserCmdSetDieMsg)
	{
		PRINT_DISABLED();
		return;
	}

	wstring wscError[] =
	{
		L"Error: Invalid parameters",
		L"Usage: /set diemsg <param>",
		L"<param>: all,system,self or none",
	};

	wstring wscDieMsg = ToLower(GetParam(wscParam, ' ', 0));;

	DIEMSGTYPE dieMsg;
	if (!wscDieMsg.compare(L"all"))
		dieMsg = DIEMSG_ALL;
	else if (!wscDieMsg.compare(L"system"))
		dieMsg = DIEMSG_SYSTEM;
	else if (!wscDieMsg.compare(L"none"))
		dieMsg = DIEMSG_NONE;
	else if (!wscDieMsg.compare(L"self"))
		dieMsg = DIEMSG_SELF;
	else
		PRINT_ERROR();

	// save to ini
	GET_USERFILE(scUserFile);
	IniWrite(scUserFile, "settings", "DieMsg", itos(dieMsg));

	// save in ClientInfo
	ClientInfo[iClientID].dieMsg = dieMsg;

	// send confirmation msg
	PRINT_OK();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UserCmd_SetDieMsgSize(uint iClientID, const wstring &wscParam)
{
	if (!set_bUserCmdSetDieMsgSize)
	{
		PRINT_DISABLED();
		return;
	}

	wstring wscError[] =
	{
		L"Error: Invalid parameters",
		L"Usage: /set diemsgsize <size>",
		L"<size>: small, default",
	};

	wstring wscDieMsgSize = ToLower(GetParam(wscParam, ' ', 0));

	CHATSIZE dieMsgSize;
	if (!wscDieMsgSize.compare(L"small"))
		dieMsgSize = CS_SMALL;
	else if (!wscDieMsgSize.compare(L"default"))
		dieMsgSize = CS_DEFAULT;
	else
		PRINT_ERROR();

	// save to ini
	GET_USERFILE(scUserFile);
	IniWrite(scUserFile, "Settings", "DieMsgSize", itos(dieMsgSize));

	// save in ClientInfo
	ClientInfo[iClientID].dieMsgSize = dieMsgSize;

	// send confirmation msg
	PRINT_OK();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UserCmd_SetChatFont(uint iClientID, const wstring &wscParam)
{
	if (!set_bUserCmdSetChatFont)
	{
		PRINT_DISABLED();
		return;
	}

	wstring wscError[] =
	{
		L"Error: Invalid parameters",
		L"Usage: /set chatfont <size> <style>",
		L"<size>: small, default or big",
		L"<style>: default, bold, italic or underline",
	};

	wstring wscChatSize = ToLower(GetParam(wscParam, ' ', 0));
	wstring wscChatStyle = ToLower(GetParam(wscParam, ' ', 1));

	CHATSIZE chatSize;
	if (!wscChatSize.compare(L"small"))
		chatSize = CS_SMALL;
	else if (!wscChatSize.compare(L"default"))
		chatSize = CS_DEFAULT;
	else if (!wscChatSize.compare(L"big"))
		chatSize = CS_BIG;
	else
		PRINT_ERROR();

	CHATSTYLE chatStyle;
	if (!wscChatStyle.compare(L"default"))
		chatStyle = CST_DEFAULT;
	else if (!wscChatStyle.compare(L"bold"))
		chatStyle = CST_BOLD;
	else if (!wscChatStyle.compare(L"italic"))
		chatStyle = CST_ITALIC;
	else if (!wscChatStyle.compare(L"underline"))
		chatStyle = CST_UNDERLINE;
	else
		PRINT_ERROR();

	// save to ini
	GET_USERFILE(scUserFile);
	IniWrite(scUserFile, "settings", "ChatSize", itos(chatSize));
	IniWrite(scUserFile, "settings", "ChatStyle", itos(chatStyle));

	// save in ClientInfo
	ClientInfo[iClientID].chatSize = chatSize;
	ClientInfo[iClientID].chatStyle = chatStyle;

	// send confirmation msg
	PRINT_OK();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UserCmd_Ignore(uint iClientID, const wstring &wscParam)
{
	if (!set_bUserCmdIgnore)
	{
		PRINT_DISABLED();
		return;
	}

	wstring wscError[] =
	{
		L"Error: Invalid parameters",
		L"Usage: /ignore <charname> [<flags>]",
		L"<charname>: character name which should be ignored(case insensitive)",
		L"<flags>: combination of the following flags:",
		L" p - only affect private chat",
		L" i - <charname> may match partially",
		L"Examples:",
		L"\"/ignore SomeDude\" ignores all chatmessages from SomeDude",
		L"\"/ignore PlayerX p\" ignores all private-chatmessages from PlayerX",
		L"\"/ignore idiot i\" ignores all chatmessages from players whose charname contain \"idiot\" (e.g. \"[XYZ]IDIOT\", \"MrIdiot\", etc)",
		L"\"/ignore Fool pi\" ignores all private-chatmessages from players whose charname contain \"fool\"",
	};

	wstring wscAllowedFlags = L"pi";

	wstring wscCharname = GetParam(wscParam, ' ', 0);
	wstring wscFlags = ToLower(GetParam(wscParam, ' ', 1));

	if (!wscCharname.length())
		PRINT_ERROR();

	// check if flags are valid
	for (uint i = 0; (i < wscFlags.length()); i++)
	{
		if (wscAllowedFlags.find_first_of(wscFlags[i]) == -1)
			PRINT_ERROR();
	}

	if (ClientInfo[iClientID].lstIgnore.size() > set_iUserCmdMaxIgnoreList)
	{
		PrintUserCmdText(iClientID, L"Error: Too many entries in the ignore list, please delete an entry first!");
		return;
	}

	// save to ini
	GET_USERFILE(scUserFile);
	IniWriteW(scUserFile, "IgnoreList", itos((int)ClientInfo[iClientID].lstIgnore.size() + 1), (wscCharname + L" " + wscFlags));

	// save in ClientInfo
	IGNORE_INFO ii;
	ii.wscCharname = wscCharname;
	ii.wscFlags = wscFlags;
	ClientInfo[iClientID].lstIgnore.push_back(ii);

	// send confirmation msg
	PRINT_OK();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UserCmd_IgnoreID(uint iClientID, const wstring &wscParam)
{
	if (!set_bUserCmdIgnore)
	{
		PRINT_DISABLED();
		return;
	}

	wstring wscError[] =
	{
		L"Error: Invalid parameters",
		L"Usage: /ignoreid <client-id> [<flags>]",
		L"<client-id>: client-id of character which should be ignored",
		L"<flags>: if \"p\"(without quotation marks) then only affect private chat",
	};

	wstring wscClientID = GetParam(wscParam, ' ', 0);
	wstring wscFlags = ToLower(GetParam(wscParam, ' ', 1));

	if (!wscClientID.length())
		PRINT_ERROR();

	if (wscFlags.length() && wscFlags.compare(L"p") != 0)
		PRINT_ERROR();

	if (ClientInfo[iClientID].lstIgnore.size() > set_iUserCmdMaxIgnoreList)
	{
		PrintUserCmdText(iClientID, L"Error: Too many entries in the ignore list, please delete an entry first!");
		return;
	}

	uint iClientIDTarget = ToInt(wscClientID);
	if (!HkIsValidClientID(iClientIDTarget) || HkIsInCharSelectMenu(iClientIDTarget))
	{
		PrintUserCmdText(iClientID, L"Error: Invalid client-id");
		return;
	}

	wstring wscCharname = (wchar_t*)Players.GetActiveCharacterName(iClientIDTarget);

	// save to ini
	GET_USERFILE(scUserFile);
	IniWriteW(scUserFile, "IgnoreList", itos((int)ClientInfo[iClientID].lstIgnore.size() + 1), (wscCharname + L" " + wscFlags));

	// save in ClientInfo
	IGNORE_INFO ii;
	ii.wscCharname = wscCharname;
	ii.wscFlags = wscFlags;
	ClientInfo[iClientID].lstIgnore.push_back(ii);

	// send confirmation msg
	PrintUserCmdText(iClientID, L"OK, \"%s\" added to ignore list", wscCharname.c_str());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UserCmd_IgnoreList(uint iClientID, const wstring &wscParam)
{
	if (!set_bUserCmdIgnore)
	{
		PRINT_DISABLED();
		return;
	}

	PrintUserCmdText(iClientID, L"ID | Charactername | Flags");
	int i = 1;
	foreach(ClientInfo[iClientID].lstIgnore, IGNORE_INFO, it)
	{
		PrintUserCmdText(iClientID, L"%.2u | %s | %s", i, it->wscCharname.c_str(), it->wscFlags.c_str());
		i++;
	}

	// send confirmation msg
	PRINT_OK();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UserCmd_DelIgnore(uint iClientID, const wstring &wscParam)
{
	if (!set_bUserCmdIgnore)
	{
		PRINT_DISABLED();
		return;
	}

	wstring wscError[] =
	{
		L"Error: Invalid parameters",
		L"Usage: /delignore <id> [<id2> <id3> ...]",
		L"<id>: id of ignore-entry(see /ignorelist) or *(delete all)",
	};

	wstring wscID = GetParam(wscParam, ' ', 0);

	if (!wscID.length())
		PRINT_ERROR();

	GET_USERFILE(scUserFile);

	if (!wscID.compare(L"*"))
	{ // delete all
		IniDelSection(scUserFile, "IgnoreList");
		ClientInfo[iClientID].lstIgnore.clear();
		PRINT_OK();
		return;
	}

	list<uint> lstDelete;
	for (uint j = 1; wscID.length(); j++)
	{
		uint iID = ToInt(wscID.c_str());
		if (!iID || (iID > ClientInfo[iClientID].lstIgnore.size()))
		{
			PrintUserCmdText(iClientID, L"Error: Invalid ID");
			return;
		}

		lstDelete.push_back(iID);
		wscID = GetParam(wscParam, ' ', j);
	}

	lstDelete.sort(greater<uint>());

	ClientInfo[iClientID].lstIgnore.reverse();
	foreach(lstDelete, uint, it)
	{
		uint iCurID = (uint)ClientInfo[iClientID].lstIgnore.size();
		foreach(ClientInfo[iClientID].lstIgnore, IGNORE_INFO, it2)
		{
			if (iCurID == (*it))
			{
				ClientInfo[iClientID].lstIgnore.erase(it2);
				break;
			}
			iCurID--;
		}
	}
	ClientInfo[iClientID].lstIgnore.reverse();

	// send confirmation msg
	IniDelSection(scUserFile, "IgnoreList");
	int i = 1;
	foreach(ClientInfo[iClientID].lstIgnore, IGNORE_INFO, it3)
	{
		IniWriteW(scUserFile, "IgnoreList", itos(i), ((*it3).wscCharname + L" " + (*it3).wscFlags));
		i++;
	}
	PRINT_OK();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UserCmd_IDs(uint iClientID, const wstring &wscParam)
{
	wchar_t wszLine[128] = L"";
	list<HKPLAYERINFO> lstPlayers = HkGetPlayers();
	foreach(lstPlayers, HKPLAYERINFO, i)
	{
		wchar_t wszBuf[1024];
		swprintf(wszBuf, L"%s = %u | ", (*i).wscCharname.c_str(), (*i).iClientID);
		if ((wcslen(wszBuf) + wcslen(wszLine)) >= sizeof(wszLine) / 2) {
			PrintUserCmdText(iClientID, L"%s", wszLine);
			wcscpy(wszLine, wszBuf);
		}
		else
			wcscat(wszLine, wszBuf);
	}

	if (wcslen(wszLine))
		PrintUserCmdText(iClientID, L"%s", wszLine);
	PrintUserCmdText(iClientID, L"OK");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UserCmd_ID(uint iClientID, const wstring &wscParam)
{
	PrintUserCmdText(iClientID, L"Your client-id: %u", iClientID);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UserCmd_Credits(uint iClientID, const wstring &wscParam)
{
	PrintUserCmdText(iClientID, L"This server is running FLHook v" VERSION);
	PrintUserCmdText(iClientID, L"Running plugins:");

	bool bRunning = false;
	foreach(lstPlugins, PLUGIN_DATA, it) {
		if (it->bPaused)
			continue;

		bRunning = true;
		PrintUserCmdText(iClientID, L"- %s", stows(it->sName).c_str());
	}
	if (!bRunning)
		PrintUserCmdText(iClientID, L"- none -");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UserCmd_Help(uint iClientID, const wstring &wscParam)
{
	if (!set_bUserCmdHelp)
	{
		PRINT_DISABLED();
		return;
	}

	bool singleCommandHelp = wscParam.length() > 1;

	if (!singleCommandHelp)
		PrintUserCmdText(iClientID, L"The following commands are available to you. Use /help <command> for detailed information.");

	wstring boldHelp = set_wscUserCmdStyle.substr(0, set_wscUserCmdStyle.length() - 1) + L"1";
	wstring normal = set_wscUserCmdStyle;

	foreach(lstHelpEntries, stHelpEntry, he) {
		if (he->fnIsDisplayed(iClientID)) {
			if (singleCommandHelp) {
				if (he->wszCommand == wscParam) {
					set_wscUserCmdStyle = boldHelp;
					PrintUserCmdText(iClientID, he->wszCommand + L" " + he->wszArguments);
					set_wscUserCmdStyle = normal;
					int pos = 0;
					while (pos != wstring::npos) {
						int nextPos = he->wszLongHelp.find('\n', pos + 1);
						PrintUserCmdText(iClientID, L"  " + he->wszLongHelp.substr(pos, (nextPos - pos)));
						pos = nextPos;
					}
					return;
				}
			}
			else {
				set_wscUserCmdStyle = boldHelp;
				PrintUserCmdText(iClientID, he->wszCommand + L" " + he->wszArguments);
				set_wscUserCmdStyle = normal;
				PrintUserCmdText(iClientID, L"  " + he->wszShortHelp);
			}
		}
	}

	if (singleCommandHelp) {
		set_wscUserCmdStyle = boldHelp;
		PrintUserCmdText(iClientID, wscParam);
		set_wscUserCmdStyle = normal;

		if (!UserCmd_Process(iClientID, wscParam))
			PrintUserCmdText(iClientID, L"No help found for specified command.");
	}
	else {
		CALL_PLUGINS_NORET(PLUGIN_UserCmd_Help, , (uint iClientID, const wstring &wscParam), (iClientID, wscParam));
	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

USERCMD UserCmds[] =
{
	{ L"/set diemsg",			UserCmd_SetDieMsg},
	{ L"/set diemsgsize",		UserCmd_SetDieMsgSize},
	{ L"/set chatfont",			UserCmd_SetChatFont},
	{ L"/ignorelist",			UserCmd_IgnoreList},
	{ L"/delignore",			UserCmd_DelIgnore},
	{ L"/ignore",				UserCmd_Ignore},
	{ L"/ignoreid",				UserCmd_IgnoreID},
	{ L"/ids",					UserCmd_IDs},
	{ L"/id",					UserCmd_ID},
	{ L"/credits",				UserCmd_Credits},
	{ L"/help",					UserCmd_Help},
};

bool UserCmd_Process(uint iClientID, const wstring &wscCmd)
{
	CALL_PLUGINS(PLUGIN_UserCmd_Process, bool, , (uint iClientID, const wstring &wscCmd), (iClientID, wscCmd));

	wstring wscCmdLower = ToLower(wscCmd);

	for (uint i = 0; (i < sizeof(UserCmds) / sizeof(USERCMD)); i++)
	{
		if (wscCmdLower.find(UserCmds[i].wszCmd) == 0)
		{
			wstring wscParam = L"";
			if (wscCmd.length() > wcslen(UserCmds[i].wszCmd))
			{
				if (wscCmd[wcslen(UserCmds[i].wszCmd)] != ' ')
					continue;
				wscParam = wscCmd.substr(wcslen(UserCmds[i].wszCmd) + 1);
			}

			// addlog
			if (set_bLogUserCmds) {
				wstring wscCharname = (wchar_t*)Players.GetActiveCharacterName(iClientID);
				HkAddUserCmdLog("%s: %s", wstos(wscCharname).c_str(), wstos(wscCmd).c_str());
			}

			try {
				UserCmds[i].proc(iClientID, wscParam);
				if (set_bLogUserCmds)
					HkAddUserCmdLog("finished");
			}
			catch (...) {
				if (set_bLogUserCmds)
					HkAddUserCmdLog("exception");
			}

			return true;
		}
	}

	return false;
}
