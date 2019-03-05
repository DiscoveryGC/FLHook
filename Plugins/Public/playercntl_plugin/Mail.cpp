// Player Control plugin for FLHookPlugin
// Feb 2010 by Cannon
//
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

#include <windows.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <FLHook.h>
#include <plugin.h>
#include <PluginUtilities.h>
#include <math.h>
#include <list>

#include "Main.h"
#include "Mail.h"

bool extern set_bLocalTime;

namespace Mail
{
	static const int MAX_MAIL_MSGS = 40;

	/** Show five messages from the specified starting position. */
	void MailShow(const wstring &wscCharname, const string &scExtension, int iFirstMsg)
	{
		// Make sure the character is logged in.
		uint iClientID = HkGetClientIdFromCharname(wscCharname);
		if (iClientID==-1)
			return;

		// Get the target player's message file.
		string scFilePath = GetUserFilePath(wscCharname, scExtension);
		if (scFilePath.length()==0)
			return;

		int iLastMsg = iFirstMsg;
		for (int iMsgSlot = iFirstMsg, iMsgCount = 0; iMsgSlot<MAX_MAIL_MSGS && iMsgCount<5; iMsgSlot++, iMsgCount++)
		{
			wstring wscTmpMsg = IniGetWS(scFilePath, "Msgs", itos(iMsgSlot), L"");
			if (wscTmpMsg.length()==0)
				break;
			PrintUserCmdText(iClientID, L"#%02d %s", iMsgSlot, wscTmpMsg.c_str());
			IniWrite(scFilePath, "MsgsRead", itos(iMsgSlot), "yes");
			iLastMsg = iMsgSlot;
		}
		PrintUserCmdText(iClientID, L"Viewing #%02d-#%02d of %02d messages", iFirstMsg, iLastMsg, MailCount(wscCharname, scExtension));
	}

	/** Return the number of unread messages. */
	int MailCountUnread(const wstring &wscCharname, const string &scExtension)
	{
		// Get the target player's message file.
		string scFilePath = GetUserFilePath(wscCharname, scExtension);
		if (scFilePath.length()==0)
			return 0;

		int iUnreadMsgs = 0;
		for (int iMsgSlot = 1; iMsgSlot<MAX_MAIL_MSGS; iMsgSlot++)
		{
			if (IniGetS(scFilePath, "Msgs", itos(iMsgSlot), "").length()==0)
				break;
			if (!IniGetB(scFilePath, "MsgsRead", itos(iMsgSlot), false))
				iUnreadMsgs++;
		}
		return iUnreadMsgs;
	}

	/** Return the number of messages. */
	int MailCount(const wstring &wscCharname, const string &scExtension)
	{
		// Get the target player's message file.
		string scFilePath = GetUserFilePath(wscCharname, scExtension);
		if (scFilePath.length()==0)
			return 0;

		int iMsgs = 0;
		for (int iMsgSlot = 1; iMsgSlot<MAX_MAIL_MSGS; iMsgSlot++)
		{
			if (IniGetS(scFilePath, "Msgs", itos(iMsgSlot), "").length()==0)
				break;
			iMsgs++;
		}
		return iMsgs;
	}


	/** Check for new or unread messages. */
	void MailCheckLog(const wstring &wscCharname, const string &scExtension)
	{
		// Make sure the character is logged in.
		uint iClientID = HkGetClientIdFromCharname(wscCharname);
		if (iClientID==-1)
			return;

		// Get the target player's message file.
		string scFilePath = GetUserFilePath(wscCharname, scExtension);
		if (scFilePath.length()==0)
			return;

		// If there are unread messaging then inform the player
		int iUnreadMsgs = MailCountUnread(wscCharname, scExtension);
		if (iUnreadMsgs>0)
		{
			PrintUserCmdText(iClientID, L"You have %d unread messages. Type /mail to see your messages", iUnreadMsgs);
		}
	}

	/**
	 Save a msg to disk so that we can inform the receiving character
	 when they log in.
	*/
	bool MailSend(const wstring &wscCharname, const string &scExtension, const wstring &wscMsg)
	{
		// Get the target player's message file.
		string scFilePath = GetUserFilePath(wscCharname, scExtension);
		if (scFilePath.length()==0)
			return false;
		
		// Move all mail up one slot starting at the end. We automatically
		// discard the oldest messages.
		for (int iMsgSlot = MAX_MAIL_MSGS-1; iMsgSlot>0; iMsgSlot--)
		{
			wstring wscTmpMsg = IniGetWS(scFilePath, "Msgs", itos(iMsgSlot), L"");
			IniWriteW(scFilePath, "Msgs", itos(iMsgSlot+1), wscTmpMsg);

			bool bTmpRead = IniGetB(scFilePath, "MsgsRead", itos(iMsgSlot), false);
			IniWrite(scFilePath, "MsgsRead", itos(iMsgSlot+1), (bTmpRead?"yes":"no"));
		}

		// Write message into the slot
		IniWriteW(scFilePath, "Msgs", "1", GetTimeString(set_bLocalTime) + L" " + wscMsg);
		IniWrite(scFilePath, "MsgsRead", "1", "no");
		return true;
	}

	/**
		Delete a message
	*/
	bool MailDel(const wstring &wscCharname, const string &scExtension, int iMsg)
	{
		// Get the target player's message file.
		string scFilePath = GetUserFilePath(wscCharname, scExtension);
		if (scFilePath.length()==0)
			return false;

		// Move all mail down one slot starting at the deleted message to overwrite it
		for (int iMsgSlot = iMsg; iMsgSlot<MAX_MAIL_MSGS; iMsgSlot++)
		{
			wstring wscTmpMsg = IniGetWS(scFilePath, "Msgs", itos(iMsgSlot+1), L"");
			IniWriteW(scFilePath, "Msgs", itos(iMsgSlot), wscTmpMsg);

			bool bTmpRead = IniGetB(scFilePath, "MsgsRead", itos(iMsgSlot+1), false);
			IniWrite(scFilePath, "MsgsRead", itos(iMsgSlot), (bTmpRead?"yes":"no"));
		}
		return true;
	}
}
