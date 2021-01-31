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

#define POPUPDIALOG_BUTTONS_LEFT_YES 1
#define POPUPDIALOG_BUTTONS_CENTER_NO 2
#define POPUPDIALOG_BUTTONS_RIGHT_LATER 4
#define POPUPDIALOG_BUTTONS_CENTER_OK 8

namespace PlayerCommands
{
	void BaseHelp(uint client, const wstring &args)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		// Separate base help out into pages. FL seems to have a limit of something like 4k per infocard.
		const uint numPages = 4;
		wstring pages[numPages];
		pages[0] = L"<TRA bold=\"true\"/><TEXT>/base help [page]</TEXT><TRA bold=\"false\"/><PARA/>"
			L"<TEXT>Show this help page. Specify the page number to see the next page.</TEXT><PARA/><PARA/>" 

			L"<TRA bold=\"true\"/><TEXT>/base login [password]</TEXT><TRA bold=\"false\"/><PARA/>"
			L"<TEXT>Login as base administrator. The following commands are only available if you are logged in as a base administrator.</TEXT><PARA/><PARA/>"

			L"<TRA bold=\"true\"/><TEXT>/base addpwd [password] [viewshop], /base rmpwd [password], /base lstpwd</TEXT><TRA bold=\"false\"/><PARA/>"
			L"<TEXT>Add, remove and list administrator passwords for the base. Add 'viewshop' to addpwd to only allow the password to view the shop.</TEXT><PARA/><PARA/>"

			L"<TRA bold=\"true\"/><TEXT>/base addtag [tag], /base rmtag [tag], /base lsttag</TEXT><TRA bold=\"false\"/><PARA/>"
			L"<TEXT>Add, remove and list ally tags for the base.</TEXT><PARA/><PARA/>"

			L"<TRA bold=\"true\"/><TEXT>/base addhostile [tag], /base rmhostile [tag], /base lsthostile</TEXT><TRA bold=\"false\"/><PARA/>"
			L"<TEXT>Add, remove and list blacklisted tags for the base. They will be shot on sight so use complete tags like =LSF= or IMG| or a shipname like Crunchy_Salad.</TEXT><PARA/><PARA/>"

			L"<TRA bold=\"true\"/><TEXT>/base setmasterpwd [old password] [new password]</TEXT><TRA bold=\"false\"/><PARA/>"
			L"<TEXT>Set the master password for the base.</TEXT><PARA/><PARA/>"

			L"<TRA bold=\"true\"/><TEXT>/base rep [clear]</TEXT><TRA bold=\"false\"/><PARA/>"
			L"<TEXT>Set or clear the faction that this base is affiliated with. When setting the affiliation, the affiliation will be that of the player executing the command.</TEXT>";

		pages[1] = L"<TRA bold=\"true\"/><TEXT>/bank withdraw [credits], /bank deposit [credits], /bank status</TEXT><TRA bold=\"false\"/><PARA/>"
			L"<TEXT>Withdraw, deposit or check the status of the credits held by the base's bank.</TEXT><PARA/><PARA/>"

			L"<TRA bold=\"true\"/><TEXT>/shop price [item] [price] [min stock] [max stock]</TEXT><TRA bold=\"false\"/><PARA/>"
			L"<TEXT>Set the [price] of [item]. If the current stock is less than [min stock]"
			L" then the item cannot be bought by docked ships. If the current stock is more or equal"
			L" to [max stock] then the item cannot be sold to the base by docked ships.</TEXT><PARA/><PARA/>"
			L"<TEXT>To prohibit selling to the base of an item by docked ships under all conditions, set [max stock] to 0."
			L"To prohibit buying from the base of an item by docked ships under all conditions, set [min stock] to 0.</TEXT><PARA/><PARA/>"

			L"<TRA bold=\"true\"/><TEXT>/shop remove [item]</TEXT><TRA bold=\"false\"/><PARA/>"
			L"<TEXT>Remove the item from the stock list. It cannot be sold to the base by docked ships unless they are base administrators.</TEXT><PARA/><PARA/>"

			L"<TRA bold=\"true\"/><TEXT>/shop [page]</TEXT><TRA bold=\"false\"/><PARA/>"
			L"<TEXT>Show the shop stock list for [page]. There are a maximum of 40 items shown per page.</TEXT>";

		pages[2] = L"<TRA bold=\"true\"/><TEXT>/base defensemode</TEXT><TRA bold=\"false\"/><PARA/>"
			L"<TEXT>Control the defense mode for the base.</TEXT><PARA/>"
			L"<TEXT>Defense Mode 1 - Logic: Blacklist > Whitelist > IFF Standing.</TEXT><PARA/>"
			L"<TEXT>Docking Rights: Whitelisted ships only.</TEXT><PARA/><PARA/>"
			L"<TEXT>Defense Mode 2 - Logic: Blacklist > Whitelist > IFF Standing.</TEXT><PARA/>"
			L"<TEXT>Docking Rights: Anyone with good standing.</TEXT><PARA/><PARA/>"
			L"<TEXT>Defense Mode 3 - Logic: Blacklist > Whitelist > Hostile</TEXT><PARA/>"
			L"<TEXT>Docking Rights: Whitelisted ships only.</TEXT><PARA/><PARA/>"
			L"<TEXT>Defense Mode 4 - Logic: Blacklist > Whitelist > Neutral</TEXT><PARA/>"
			L"<TEXT>Docking Rights: Anyone with good standing.</TEXT><PARA/><PARA/>"
			L"<TEXT>Defense Mode 5 - Logic: Blacklist > Whitelist > Neutral</TEXT><PARA/>"
			L"<TEXT>Docking Rights: Whitelisted ships only.</TEXT><PARA/><PARA/>"

			L"<TRA bold=\"true\"/><TEXT>/base info</TEXT><TRA bold=\"false\"/><PARA/>"
			L"<TEXT>Set the base's infocard description.</TEXT>";

		pages[3] = L"<TRA bold=\"true\"/><TEXT>/base facmod</TEXT><TRA bold=\"false\"/><PARA/>"
			L"<TEXT>Control factory modules.</TEXT><PARA/><PARA/>"

			L"<TRA bold=\"true\"/><TEXT>/base defmod</TEXT><TRA bold=\"false\"/><PARA/>"
			L"<TEXT>Control defense modules.</TEXT><PARA/><PARA/>"

			L"<TRA bold=\"true\"/><TEXT>/base shieldmod</TEXT><TRA bold=\"false\"/><PARA/>"
			L"<TEXT>Control shield modules.</TEXT><PARA/><PARA/>"

			L"<TRA bold=\"true\"/><TEXT>/base buildmod</TEXT><TRA bold=\"false\"/><PARA/>"
			L"<TEXT>Control the construction and destruction of base modules and upgrades.</TEXT>";

		uint page = 0;
		wstring pageNum = GetParam(args, ' ', 2);
		if (pageNum.length())
		{
			page = ToUInt(pageNum) - 1;
			if (page < 0 || page > numPages - 1) {
				page = 0;
			}
		}

		wstring pagetext = pages[page];

		wchar_t titleBuf[4000];
		_snwprintf(titleBuf, sizeof(titleBuf), L"Base Help : Page %d/%d", page + 1, numPages);

		wchar_t buf[4000];
		_snwprintf(buf, sizeof(buf), L"<RDL><PUSH/>%s<POP/></RDL>", pagetext);

		HkChangeIDSString(client, 500000, titleBuf);
		HkChangeIDSString(client, 500001, buf);

		FmtStr caption(0, 0);
		caption.begin_mad_lib(500000);
		caption.end_mad_lib();

		FmtStr message(0, 0);
		message.begin_mad_lib(500001);
		message.end_mad_lib();

		pub::Player::PopUpDialog(client, caption, message, POPUPDIALOG_BUTTONS_CENTER_OK);
	}

	bool RateLimitLogins(uint client, PlayerBase *base, wstring charname)
	{
		uint curr_time = (uint)time(0);
		uint big_penalty_time = 300;
		uint amount_of_attempts_to_reach_penalty = 15;

		//initiate
		if (base->unsuccessful_logins_in_a_row.find(charname) == base->unsuccessful_logins_in_a_row.end())
			base->unsuccessful_logins_in_a_row[charname] = 0;

		if (base->last_login_attempt_time.find(charname) == base->last_login_attempt_time.end())
			base->last_login_attempt_time[charname] = 0;

		//nulify counter if more than N seconds passed.
		if ((curr_time - base->last_login_attempt_time[charname]) > big_penalty_time)
			base->unsuccessful_logins_in_a_row[charname] = 0;

		uint blocktime = 1;
		if (base->unsuccessful_logins_in_a_row[charname] >= amount_of_attempts_to_reach_penalty)
			blocktime = big_penalty_time;

		uint waittime = blocktime - (curr_time - base->last_login_attempt_time[charname]);
		//You are attempting to log in too often
		if ((curr_time - base->last_login_attempt_time[charname]) < blocktime)
		{
			PrintUserCmdText(client, L"ERR You are attempting to log in too often. %d unsuccesful attempts. Wait %d seconds before repeating attempt.", base->unsuccessful_logins_in_a_row[charname], waittime);
			return true;
		} 
		
		if (base->unsuccessful_logins_in_a_row[charname] >= amount_of_attempts_to_reach_penalty) 
			base->unsuccessful_logins_in_a_row[charname] = 0;

		return false;
	}

	void BaseLogin(uint client, const wstring &args)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);

		//prevent too often login attempts
		wstring charname = (const wchar_t*)Players.GetActiveCharacterName(client);
		if (RateLimitLogins(client, base, charname)) return;

		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		wstring password = GetParam(args, ' ', 2);
		if (!password.length())
		{
			PrintUserCmdText(client, L"ERR No password");
			return;
		}

		//remember last time attempt to login
		base->last_login_attempt_time[charname] = (uint)time(0);

		BasePassword searchBp;
		searchBp.pass = password;
		list<BasePassword>::iterator ret = find(base->passwords.begin(), base->passwords.end(), searchBp);
		if (ret == base->passwords.end()) {
			base->unsuccessful_logins_in_a_row[charname]++; //count password failures
			PrintUserCmdText(client, L"ERR Access denied");
			return;
		}

		BasePassword foundBp = *ret;
		if (foundBp.admin) {
			clients[client].admin = true;
			SendMarketGoodSync(base, client);
			PrintUserCmdText(client, L"OK Access granted");
			PrintUserCmdText(client, L"Welcome administrator, all base command and control functions are available.");
		}
		if (foundBp.viewshop) {
			clients[client].viewshop = true;
			PrintUserCmdText(client, L"OK Access granted");
			PrintUserCmdText(client, L"Welcome shop viewer.");
		}

	}

	void BaseAddPwd(uint client, const wstring &args)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		if (!clients[client].admin)
		{
			PrintUserCmdText(client, L"ERR Access denied");
			return;
		}

		wstring password = GetParam(args, ' ', 2);
		if (!password.length())
		{
			PrintUserCmdText(client, L"ERR No password");
			return;
		}

		BasePassword searchBp;
		searchBp.pass = password;

		if (find(base->passwords.begin(), base->passwords.end(), searchBp) != base->passwords.end())
		{
			PrintUserCmdText(client, L"ERR Password already exists");
			return;
		}

		BasePassword bp;
		bp.pass = password;

		wstring flagsStr = GetParam(args, ' ', 3);
		int flags = 0;
		if (flagsStr.length() && flagsStr == L"viewshop")
		{
			bp.viewshop = true;
		}
		else {
			bp.admin = true;
		}

		base->passwords.push_back(bp);
		base->Save();
		PrintUserCmdText(client, L"OK");
	}

	void BaseRmPwd(uint client, const wstring &args)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		if (!clients[client].admin)
		{
			PrintUserCmdText(client, L"ERR Access denied");
			return;
		}

		wstring password = GetParam(args, ' ', 2);
		if (!password.length())
		{
			PrintUserCmdText(client, L"ERR No password");
		}

		BasePassword searchBp;
		searchBp.pass = password;
		list<BasePassword>::iterator ret = find(base->passwords.begin(), base->passwords.end(), searchBp);
		if (ret != base->passwords.end())
		{
			BasePassword bp = *ret;
			base->passwords.remove(bp);
			base->Save();
			PrintUserCmdText(client, L"OK");
			return;
		}

		PrintUserCmdText(client, L"ERR Password does not exist");
	}

	void BaseSetMasterPwd(uint client, const wstring &args)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		if (!clients[client].admin)
		{
			PrintUserCmdText(client, L"ERR Access denied");
			return;
		}

		wstring old_password = GetParam(args, ' ', 2);
		if (!old_password.length())
		{
			PrintUserCmdText(client, L"ERR No old password");
			PrintUserCmdText(client, L"/base setmasterpwd <old_password> <new_password>");
			return;
		}

		wstring new_password = GetParam(args, ' ', 3);
		if (!new_password.length())
		{
			PrintUserCmdText(client, L"ERR No new password");
			PrintUserCmdText(client, L"/base setmasterpwd <old_password> <new_password>");
			return;
		}

		BasePassword bp;
		bp.pass = new_password;
		bp.admin = true;

		if (find(base->passwords.begin(), base->passwords.end(), bp) != base->passwords.end())
		{
			PrintUserCmdText(client, L"ERR Password already exists");
			return;
		}

		if (base->passwords.size())
		{
			if (base->passwords.front().pass != old_password)
			{
				PrintUserCmdText(client, L"ERR Incorrect master password");
				PrintUserCmdText(client, L"/base setmasterpwd <old_password> <new_password>");
				return;
			}
		}

		base->passwords.remove(base->passwords.front());
		base->passwords.push_front(bp);
		base->Save();
		PrintUserCmdText(client, L"OK New master password %s", new_password.c_str());
	}

	void BaseLstPwd(uint client, const wstring &cmd)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		if (!clients[client].admin)
		{
			PrintUserCmdText(client, L"ERR Access denied");
			return;
		}

		// Do not display the first password.
		bool first = true;
		foreach(base->passwords, BasePassword, bpi)
		{
			if (first)
				first = false;
			else {
				BasePassword bp = *bpi;
				wstring *p = &(bp.pass);
				if (bp.admin)
				{
					PrintUserCmdText(client, L"%s - admin", p->c_str());
				}
				if (bp.viewshop)
				{
					PrintUserCmdText(client, L"%s - viewshop", p->c_str());
				}
			}
		}
		PrintUserCmdText(client, L"OK");
	}

	void BaseAddAllyTag(uint client, const wstring &args)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		if (!clients[client].admin)
		{
			PrintUserCmdText(client, L"ERR Access denied");
			return;
		}

		wstring tag = GetParam(args, ' ', 2);
		if (!tag.length())
		{
			PrintUserCmdText(client, L"ERR No tag");
			return;
		}

		if (find(base->ally_tags.begin(), base->ally_tags.end(), tag) != base->ally_tags.end())
		{
			PrintUserCmdText(client, L"ERR Tag already exists");
			return;
		}

		base->ally_tags.push_back(tag);

		// Logging
		wstring thecharname = (const wchar_t*)Players.GetActiveCharacterName(client);
		wstring wscMsg = L": \"%sender\" added \"%victim\" to whitelist of base \"%base\"";
		wscMsg = ReplaceStr(wscMsg, L"%sender", thecharname.c_str());
		wscMsg = ReplaceStr(wscMsg, L"%victim", tag);
		wscMsg = ReplaceStr(wscMsg, L"%base", base->basename);
		string scText = wstos(wscMsg);
		BaseLogging("%s", scText.c_str());

		base->Save();
		PrintUserCmdText(client, L"OK");
	}


	void BaseRmAllyTag(uint client, const wstring &args)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		if (!clients[client].admin)
		{
			PrintUserCmdText(client, L"ERR Access denied");
			return;
		}

		wstring tag = GetParam(args, ' ', 2);
		if (!tag.length())
		{
			PrintUserCmdText(client, L"ERR No tag");
		}

		if (find(base->ally_tags.begin(), base->ally_tags.end(), tag) == base->ally_tags.end())
		{
			PrintUserCmdText(client, L"ERR Tag does not exist");
			return;
		}

		base->ally_tags.remove(tag);

		// Logging
		wstring thecharname = (const wchar_t*)Players.GetActiveCharacterName(client);
		wstring wscMsg = L": \"%sender\" removed \"%victim\" from whitelist of base \"%base\"";
		wscMsg = ReplaceStr(wscMsg, L"%sender", thecharname.c_str());
		wscMsg = ReplaceStr(wscMsg, L"%victim", tag);
		wscMsg = ReplaceStr(wscMsg, L"%base", base->basename);
		string scText = wstos(wscMsg);
		BaseLogging("%s", scText.c_str());

		base->Save();
		PrintUserCmdText(client, L"OK");
	}

	void BaseLstAllyTag(uint client, const wstring &cmd)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		if (!clients[client].admin)
		{
			PrintUserCmdText(client, L"ERR Access denied");
			return;
		}

		foreach(base->ally_tags, wstring, i)
			PrintUserCmdText(client, L"%s", i->c_str());
		PrintUserCmdText(client, L"OK");
	}

	void BaseRep(uint client, const wstring &args)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		bool isServerAdmin;

		wstring rights;
		if (HkGetAdmin((const wchar_t*)Players.GetActiveCharacterName(client), rights) == HKE_OK && rights.find(L"superadmin") != -1)
		{
			isServerAdmin = true;
		}


		if (!clients[client].admin && !isServerAdmin)
		{
			PrintUserCmdText(client, L"ERR Access denied");
			return;
		}

		wstring arg = GetParam(args, ' ', 2);
		if (arg == L"clear")
		{
			if (isServerAdmin) {
				base->affiliation = 0;
				base->Save();
				PrintUserCmdText(client, L"OK cleared base reputation");
			}
			else
			{
				PrintUserCmdText(client, L"ERR Cannot clear affiliation, please contact administration team");
			}
			return;
		}

		if (isServerAdmin || base->affiliation <= 0)
		{
			int rep;
			pub::Player::GetRep(client, rep);

			uint affiliation;
			Reputation::Vibe::Verify(rep);
			Reputation::Vibe::GetAffiliation(rep, affiliation, false);
			if (affiliation == -1)
			{
				PrintUserCmdText(client, L"OK Player has no affiliation");
				return;
			}

			base->affiliation = affiliation;
			base->Save();
			PrintUserCmdText(client, L"OK Affiliation set to %s", HkGetWStringFromIDS(Reputation::get_name(affiliation)).c_str());
		}
		else
		{
			PrintUserCmdText(client, L"ERR Cannot set affiliation once it's been set, please contact administration team");
		}
	}

	void BaseAddHostileTag(uint client, const wstring &args)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		if (!clients[client].admin)
		{
			PrintUserCmdText(client, L"ERR Access denied");
			return;
		}

		wstring tag = GetParam(args, ' ', 2);
		if (!tag.length())
		{
			PrintUserCmdText(client, L"ERR No tag");
			return;
		}

		if (find(base->perma_hostile_tags.begin(), base->perma_hostile_tags.end(), tag) != base->perma_hostile_tags.end())
		{
			PrintUserCmdText(client, L"ERR Tag already exists");
			return;
		}


		base->perma_hostile_tags.push_back(tag);

		// Logging
		wstring thecharname = (const wchar_t*)Players.GetActiveCharacterName(client);
		wstring wscMsg = L": \"%sender\" added \"%victim\" to blacklist of base \"%base\"";
		wscMsg = ReplaceStr(wscMsg, L"%sender", thecharname.c_str());
		wscMsg = ReplaceStr(wscMsg, L"%victim", tag);
		wscMsg = ReplaceStr(wscMsg, L"%base", base->basename);
		string scText = wstos(wscMsg);
		BaseLogging("%s", scText.c_str());

		base->Save();

		PrintUserCmdText(client, L"OK");
	}


	void BaseRmHostileTag(uint client, const wstring &args)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		if (!clients[client].admin)
		{
			PrintUserCmdText(client, L"ERR Access denied");
			return;
		}

		wstring tag = GetParam(args, ' ', 2);
		if (!tag.length())
		{
			PrintUserCmdText(client, L"ERR No tag");
		}

		if (find(base->perma_hostile_tags.begin(), base->perma_hostile_tags.end(), tag) == base->perma_hostile_tags.end())
		{
			PrintUserCmdText(client, L"ERR Tag does not exist");
			return;
		}

		base->perma_hostile_tags.remove(tag);

		// Logging
		wstring thecharname = (const wchar_t*)Players.GetActiveCharacterName(client);
		wstring wscMsg = L": \"%sender\" removed \"%victim\" from blacklist of base \"%base\"";
		wscMsg = ReplaceStr(wscMsg, L"%sender", thecharname.c_str());
		wscMsg = ReplaceStr(wscMsg, L"%victim", tag);
		wscMsg = ReplaceStr(wscMsg, L"%base", base->basename);
		string scText = wstos(wscMsg);
		BaseLogging("%s", scText.c_str());


		base->Save();

		PrintUserCmdText(client, L"OK");
	}

	void BaseLstHostileTag(uint client, const wstring &cmd)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		if (!clients[client].admin)
		{
			PrintUserCmdText(client, L"ERR Access denied");
			return;
		}

		foreach(base->perma_hostile_tags, wstring, i)
			PrintUserCmdText(client, L"%s", i->c_str());
		PrintUserCmdText(client, L"OK");
	}

	void BaseInfo(uint client, const wstring &args)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		if (!clients[client].admin)
		{
			PrintUserCmdText(client, L"ERR Access denied");
			return;
		}

		uint iPara = ToInt(GetParam(args, ' ', 2));
		const wstring &cmd = GetParam(args, ' ', 3);
		const wstring &msg = GetParamToEnd(args, ' ', 4);

		if (iPara > 0 && iPara <= MAX_PARAGRAPHS && cmd == L"a")
		{
			int length = base->infocard_para[iPara].length() + msg.length();
			if (length > MAX_CHARACTERS)
			{
				PrintUserCmdText(client, L"ERR Too many characters. Limit is %d", MAX_CHARACTERS);
				return;
			}

			base->infocard_para[iPara] += XMLText(msg);
			PrintUserCmdText(client, L"OK %d/%d characters used", length, MAX_CHARACTERS);

			// Update the infocard text.
			base->infocard.clear();
			for (int i = 1; i <= MAX_PARAGRAPHS; i++)
			{
				wstring wscXML = base->infocard_para[i];
				if (wscXML.length())
					base->infocard += L"<TEXT>" + wscXML + L"</TEXT><PARA/><PARA/>";
			}

			base->Save();
		}
		else if (iPara > 0 && iPara <= MAX_PARAGRAPHS && cmd == L"d")
		{
			base->infocard_para[iPara] = L"";
			PrintUserCmdText(client, L"OK");

			// Update the infocard text.
			base->infocard.clear();
			for (int i = 1; i <= MAX_PARAGRAPHS; i++)
			{
				wstring wscXML = base->infocard_para[i];
				if (wscXML.length())
					base->infocard += L"<TEXT>" + wscXML + L"</TEXT><PARA/><PARA/>";
			}

			base->Save();
		}
		else
		{
			PrintUserCmdText(client, L"ERR Invalid parameters");
			PrintUserCmdText(client, L"/base info <paragraph> <command> <text>");
			PrintUserCmdText(client, L"|  <paragraph> The paragraph number in the range 1-%d", MAX_PARAGRAPHS);
			PrintUserCmdText(client, L"|  <command> The command to perform on the paragraph, 'a' for append, 'd' for delete");
		}
	}

	void BaseDefenseMode(uint client, const wstring &args)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		if (!clients[client].admin)
		{
			PrintUserCmdText(client, L"ERR Access denied");
			return;
		}

		wstring wscMode = GetParam(args, ' ', 2);
		if (wscMode == L"1")
		{
			base->defense_mode = 1;
		}
		else if (wscMode == L"2")
		{
			base->defense_mode = 2;
		}
		else if (wscMode == L"3")
		{
			base->defense_mode = 3;
		}
		else if (wscMode == L"4")
		{
			base->defense_mode = 4;
		}
		else if (wscMode == L"5")
		{
			base->defense_mode = 5;
		}
		else
		{
			PrintUserCmdText(client, L"/base defensemode <mode>");
			PrintUserCmdText(client, L"|  <mode> = 1 - Logic: Blacklist > Whitelist > IFF Standing. | Docking Rights: Whitelisted ships only.");
			PrintUserCmdText(client, L"|  <mode> = 2 - Logic: Blacklist > Whitelist > IFF Standing. | Docking Rights: Anyone with good standing.");
			PrintUserCmdText(client, L"|  <mode> = 3 - Logic: Blacklist > Whitelist > Hostile       | Docking Rights: Whitelisted ships only.");
			PrintUserCmdText(client, L"|  <mode> = 4 - Logic: Blacklist > Whitelist > Neutral       | Docking Rights: Anyone with good standing.");
			PrintUserCmdText(client, L"|  <mode> = 5 - Logic: Blacklist > Whitelist > Neutral       | Docking Rights: Whitelisted ships only.");
			PrintUserCmdText(client, L"defensemode = %u", base->defense_mode);
			return;
		}

		PrintUserCmdText(client, L"OK defensemode = %u", base->defense_mode);

		base->Save();

		base->SyncReputationForBase();
	}

	void BaseBuildMod(uint client, const wstring &args)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		if (!clients[client].admin)
		{
			PrintUserCmdText(client, L"ERR Access denied");
			return;
		}

		const wstring &cmd = GetParam(args, ' ', 2);
		if (cmd == L"list")
		{
			PrintUserCmdText(client, L"Modules:");
			for (uint index = 1; index < base->modules.size(); index++)
			{
				if (base->modules[index])
				{
					Module *mod = (Module*)base->modules[index];
					PrintUserCmdText(client, L"%u: %s", index, mod->GetInfo(false).c_str());
				}
				else
				{
					PrintUserCmdText(client, L"%u: Empty - available for new module", index);
				}
			}
			PrintUserCmdText(client, L"OK");
		}
		else if (cmd == L"destroy")
		{
			uint index = ToInt(GetParam(args, ' ', 3));
			if (index < 1 || index >= base->modules.size() || !base->modules[index])
			{
				PrintUserCmdText(client, L"ERR Module not found");
				return;
			}

			if (base->modules[index]->type == Module::TYPE_STORAGE && base->GetRemainingCargoSpace() < STORAGE_MODULE_CAPACITY)
			{
				PrintUserCmdText(client, L"ERR Need %d free space to destroy a storage module", STORAGE_MODULE_CAPACITY);

				wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(client);
				pub::Player::SendNNMessage(client, pub::GetNicknameId("nnv_anomaly_detected"));
				wstring wscMsgU = L"KITTY ALERT: Possible type 5 POB cheating by %name (Index = %index, RemainingSpace = %space)\n";
				wscMsgU = ReplaceStr(wscMsgU, L"%name", wscCharname.c_str());
				wscMsgU = ReplaceStr(wscMsgU, L"%index", stows(itos(index)).c_str());
				wscMsgU = ReplaceStr(wscMsgU, L"%space", stows(itos((int)base->GetRemainingCargoSpace())).c_str());

				ConPrint(wscMsgU);
				LogCheater(client, wscMsgU);

				return;
			}

			delete base->modules[index];
			base->modules[index] = 0;
			base->Save();
			PrintUserCmdText(client, L"OK Module destroyed");
		}
		else if (cmd == L"construct")
		{
			uint index = ToInt(GetParam(args, ' ', 3));
			uint type = ToInt(GetParam(args, ' ', 4));
			if (index < 1 || index >= base->modules.size() || base->modules[index])
			{
				PrintUserCmdText(client, L"ERR Module index not valid");
				return;
			}

			if (type < Module::TYPE_CORE || type > Module::TYPE_LAST)
			{
				PrintUserCmdText(client, L"ERR Module type not available");
				return;
			}

			if (type == Module::TYPE_CORE)
			{
				if (base->base_level >= 4)
				{
					PrintUserCmdText(client, L"ERR Upgrade not available");
					return;
				}
			}

			//make the nickname for inspection
			uint module_nickname = CreateID(MODULE_TYPE_NICKNAMES[type]);

			if (recipes[module_nickname].reqlevel > base->base_level)
			{
				PrintUserCmdText(client, L"ERR Insufficient Core Level");
				return;
			}

			base->modules[index] = new BuildModule(base, type);
			base->Save();
			PrintUserCmdText(client, L"OK Module construction started");
		}
		else
		{
			PrintUserCmdText(client, L"ERR Invalid parameters");
			PrintUserCmdText(client, L"/base buildmod [list|construct|destroy]");
			PrintUserCmdText(client, L"|  list - show modules and build status");
			PrintUserCmdText(client, L"|  destroy <index> - destroy module at <index>");
			PrintUserCmdText(client, L"|  construct <index> <type> - start building module <type> at <index>");
			PrintUserCmdText(client, L"|     <type> = 1 - core upgrade");
			PrintUserCmdText(client, L"|     <type> = 2 - shield generator");
			PrintUserCmdText(client, L"|     <type> = 3 - cargo storage");
			PrintUserCmdText(client, L"|     <type> = 4 - defense platform array type 1");
			PrintUserCmdText(client, L"|     <type> = 5 - docking module factory");
			PrintUserCmdText(client, L"|     <type> = 6 - jumpdrive manufacturing factory");
			PrintUserCmdText(client, L"|     <type> = 7 - hyperspace survey manufacturing factory");
			PrintUserCmdText(client, L"|     <type> = 8 - cloaking device manufacturing factory");
			PrintUserCmdText(client, L"|     <type> = 9 - defense platform array type 2");
			PrintUserCmdText(client, L"|     <type> = 10 - defense platform array type 3");
			PrintUserCmdText(client, L"|     <type> = 11 - Cloak Disruptor Factory");
		}
	}

	void BaseFacMod(uint client, const wstring &args)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		if (!clients[client].admin)
		{
			PrintUserCmdText(client, L"ERR Access denied");
			return;
		}

		const wstring &cmd = GetParam(args, ' ', 2);
		if (cmd == L"list")
		{
			PrintUserCmdText(client, L"Factory Modules:");
			for (uint index = 1; index < base->modules.size(); index++)
			{
				if (base->modules[index] &&
					(base->modules[index]->type == Module::TYPE_M_CLOAK
						|| base->modules[index]->type == Module::TYPE_M_HYPERSPACE_SCANNER
						|| base->modules[index]->type == Module::TYPE_M_JUMPDRIVES
						|| base->modules[index]->type == Module::TYPE_M_DOCKING
						|| base->modules[index]->type == Module::TYPE_M_CLOAKDISRUPTOR))
				{
					FactoryModule *mod = (FactoryModule*)base->modules[index];
					PrintUserCmdText(client, L"%u: %s", index, mod->GetInfo(false).c_str());
				}
			}
			PrintUserCmdText(client, L"OK");
		}
		else if (cmd == L"clear")
		{
			uint index = ToInt(GetParam(args, ' ', 3));
			if (index < 1 || index >= base->modules.size() || !base->modules[index])
			{
				PrintUserCmdText(client, L"ERR Module index not valid");
				return;
			}

			if (!base->modules[index] ||
				(base->modules[index]->type != Module::TYPE_M_CLOAK
					&& base->modules[index]->type != Module::TYPE_M_HYPERSPACE_SCANNER
					&& base->modules[index]->type != Module::TYPE_M_JUMPDRIVES
					&& base->modules[index]->type != Module::TYPE_M_DOCKING
					&& base->modules[index]->type != Module::TYPE_M_CLOAKDISRUPTOR))
			{
				PrintUserCmdText(client, L"ERR Not factory module");
				return;
			}

			FactoryModule *mod = (FactoryModule*)base->modules[index];
			if (mod->ClearQueue())
				PrintUserCmdText(client, L"OK Build queue cleared");
			else
				PrintUserCmdText(client, L"ERR Build queue clear failed");
			base->Save();
		}
		else if (cmd == L"cancel")
		{
			uint index = ToInt(GetParam(args, ' ', 3));
			if (index < 1 || index >= base->modules.size() || !base->modules[index])
			{
				PrintUserCmdText(client, L"ERR Module index not valid");
				return;
			}

			if (!base->modules[index] ||
				(base->modules[index]->type != Module::TYPE_M_CLOAK
					&& base->modules[index]->type != Module::TYPE_M_HYPERSPACE_SCANNER
					&& base->modules[index]->type != Module::TYPE_M_JUMPDRIVES
					&& base->modules[index]->type != Module::TYPE_M_DOCKING
					&& base->modules[index]->type != Module::TYPE_M_CLOAKDISRUPTOR))
			{
				PrintUserCmdText(client, L"ERR Not factory module");
				return;
			}

			FactoryModule *mod = (FactoryModule*)base->modules[index];
			mod->ClearRecipe();
			PrintUserCmdText(client, L"OK Active recipe is canceled");
			base->Save();
		}
		else if (cmd == L"add")
		{
			uint index = ToInt(GetParam(args, ' ', 3));
			uint type = ToInt(GetParam(args, ' ', 4));
			if (index < 1 || index >= base->modules.size() || !base->modules[index])
			{
				PrintUserCmdText(client, L"ERR Module index not valid");
				return;
			}

			if (!base->modules[index] ||
				(base->modules[index]->type != Module::TYPE_M_CLOAK
					&& base->modules[index]->type != Module::TYPE_M_HYPERSPACE_SCANNER
					&& base->modules[index]->type != Module::TYPE_M_JUMPDRIVES
					&& base->modules[index]->type != Module::TYPE_M_DOCKING
					&& base->modules[index]->type != Module::TYPE_M_CLOAKDISRUPTOR))
			{
				PrintUserCmdText(client, L"ERR Not factory module");
				return;
			}

			FactoryModule *mod = (FactoryModule*)base->modules[index];
			if (mod->AddToQueue(type))
				PrintUserCmdText(client, L"OK Item added to build queue");
			else
				PrintUserCmdText(client, L"ERR Item add to build queue failed");
			base->Save();
		}
		else
		{
			PrintUserCmdText(client, L"ERR Invalid parameters");
			PrintUserCmdText(client, L"/base facmod [list|clear|cancel|add]");
			PrintUserCmdText(client, L"|  list - show factory modules and build status");
			PrintUserCmdText(client, L"|  clear <index> - clear queue, which starts from the second item in the building queue for the factory module at <index>");

			PrintUserCmdText(client, L"|  cancel <index> - clear only active recipe, which is the first item in the building queue for the factory module at <index>");
			
			PrintUserCmdText(client, L"|  add <index> <type> - add item <type> to build queue for factory module at <index>");
			PrintUserCmdText(client, L"|     For Docking Module Factory:");
			PrintUserCmdText(client, L"|     <type> = 1 - docking module type 1");
			PrintUserCmdText(client, L"|     For Hyperspace Jumpdrive Factory");
			PrintUserCmdText(client, L"|     <type> = 2 - Jump Drive Series II");
			PrintUserCmdText(client, L"|     <type> = 3 - Jump Drive Series III");
			PrintUserCmdText(client, L"|     <type> = 4 - Jump Drive Series IV");
			PrintUserCmdText(client, L"|     For Hyperspace Survey Factory");
			PrintUserCmdText(client, L"|     <type> = 5 - Hyperspace Survey Module Mk1");
			PrintUserCmdText(client, L"|     <type> = 6 - Hyperspace Survey Module Mk2");
			PrintUserCmdText(client, L"|     <type> = 7 - Hyperspace Survey Module Mk3");
			PrintUserCmdText(client, L"|     <type> = 15 - Hyperspace Matrix Mk1");
			PrintUserCmdText(client, L"|     For Cloaking Device Factory");
			PrintUserCmdText(client, L"|     <type> = 8 - Cloaking Device MK1 (small)");
			PrintUserCmdText(client, L"|     <type> = 9 - Cloaking Device MK2 (medium)");
			PrintUserCmdText(client, L"|     <type> = 10 - Cloaking Device MK2 Advanced (large)");
			PrintUserCmdText(client, L"|     <type> = 11 - Cloaking Device MK3 (transport)");
			PrintUserCmdText(client, L"|     For Cloak Disruptor Factory");
			PrintUserCmdText(client, L"|     <type> = 12 - Cloak Disruptor Type-1");
			PrintUserCmdText(client, L"|     <type> = 13 - Cloak Disruptor Type-2");
			PrintUserCmdText(client, L"|     <type> = 14 - Cloak Disruptor Type-3");
		}
	}

	void BaseDefMod(uint client, const wstring &args)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		if (!clients[client].admin)
		{
			PrintUserCmdText(client, L"ERR Access denied");
			return;
		}

		const wstring &cmd = GetParam(args, ' ', 2);
		if (cmd == L"list")
		{
			PrintUserCmdText(client, L"Defense Modules:");
			for (uint index = 0; index < base->modules.size(); index++)
			{
				if (base->modules[index])
				{
					if (base->modules[index]->type == Module::TYPE_DEFENSE_1
						|| base->modules[index]->type == Module::TYPE_DEFENSE_2
						|| base->modules[index]->type == Module::TYPE_DEFENSE_3)
					{
						DefenseModule *mod = (DefenseModule*)base->modules[index];
						PrintUserCmdText(client, L"Module %u: Position %0.0f %0.0f %0.0f Orient %0.0f %0.0f %0.0f",
							index, mod->pos.x, mod->pos.y, mod->pos.z,
							mod->rot.z, mod->rot.y, mod->rot.z);
					}
				}
			}
			PrintUserCmdText(client, L"OK");
		}
		else if (cmd == L"set")
		{
			uint index = ToInt(GetParam(args, ' ', 3));
			float x = (float)ToInt(GetParam(args, ' ', 4));
			float y = (float)ToInt(GetParam(args, ' ', 5));
			float z = (float)ToInt(GetParam(args, ' ', 6));
			float rx = (float)ToInt(GetParam(args, ' ', 7));
			float ry = (float)ToInt(GetParam(args, ' ', 8));
			float rz = (float)ToInt(GetParam(args, ' ', 9));
			if (index < base->modules.size() && base->modules[index])
			{
				if (base->modules[index]->type == Module::TYPE_DEFENSE_1
					|| base->modules[index]->type == Module::TYPE_DEFENSE_2
					|| base->modules[index]->type == Module::TYPE_DEFENSE_3)
				{
					DefenseModule *mod = (DefenseModule*)base->modules[index];

					// Distance from base is limited to 5km
					Vector new_pos = { x, y, z };
					if (HkDistance3D(new_pos, base->position) > 5000)
					{
						PrintUserCmdText(client, L"ERR Out of range");
						return;
					}

					mod->pos = new_pos;
					mod->rot.x = rx;
					mod->rot.y = ry;
					mod->rot.z = rz;

					PrintUserCmdText(client, L"OK Module %u: Position %0.0f %0.0f %0.0f Orient %0.0f %0.0f %0.0f",
						index, mod->pos.x, mod->pos.y, mod->pos.z,
						mod->rot.z, mod->rot.y, mod->rot.z);
					base->Save();
					mod->Reset();
				}
				else
				{
					PrintUserCmdText(client, L"ERR Module not found");
				}
			}
			else
			{
				PrintUserCmdText(client, L"ERR Module not found");
			}
		}
		else
		{
			PrintUserCmdText(client, L"ERR Invalid parameters");
			PrintUserCmdText(client, L"/base defmod [list|set]");
			PrintUserCmdText(client, L"|  list - show position and orientations of this bases weapons platform");
			PrintUserCmdText(client, L"|  set - <index> <x> <y> <z> <rx> <ry> <rz> - set the position and orientation of the <index> weapons platform, where x,y,z is the position and rx,ry,rz is the orientation");
		}
	}

	void BaseShieldMod(uint client, const wstring &args)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		if (!clients[client].admin)
		{
			PrintUserCmdText(client, L"ERR Access denied");
			return;
		}

		const wstring &cmd = GetParam(args, ' ', 2);
		if (cmd == L"on")
		{
			base->shield_active_time = 3600 * 24;
		}
		else if (cmd == L"off")
		{
			base->shield_active_time = 0;
		}
		else
		{
			PrintUserCmdText(client, L"ERR Invalid parameters");
			PrintUserCmdText(client, L"/base shieldmod [on|off]");
			PrintUserCmdText(client, L"|  on - turn the shield on");
			PrintUserCmdText(client, L"|  off - turn the shield off");
		}

		// Force the timer for the shield module(s) to run and read their
		// status.
		for (uint index = 0; index < base->modules.size(); index++)
		{
			if (base->modules[index] &&
				base->modules[index]->type == Module::TYPE_SHIELDGEN)
			{
				ShieldModule *mod = (ShieldModule*)base->modules[index];
				mod->Timer(0);
				PrintUserCmdText(client, L"|  * %s", mod->GetInfo(false).c_str());
			}
		}
		PrintUserCmdText(client, L"OK");
	}

	void Bank(uint client, const wstring &args)
	{
		PlayerBase *base = GetPlayerBaseForClient(client);

		const wstring &cmd = GetParam(args, ' ', 1);
		int money = ToInt(GetParam(args, ' ', 2));

		wstring charname = (const wchar_t*)Players.GetActiveCharacterName(client);

		if (cmd == L"withdraw")
		{
			if (!clients[client].admin)
			{
				PrintUserCmdText(client, L"ERR Access denied");
				return;
			}

			float fValue;
			pub::Player::GetAssetValue(client, fValue);

			int iCurrMoney;
			pub::Player::InspectCash(client, iCurrMoney);

			if (fValue + money > 2100000000 || iCurrMoney + money > 2100000000)
			{
				PrintUserCmdText(client, L"ERR Ship asset value will be exceeded");
				return;
			}

			if (money > base->money || money < 0)
			{
				PrintUserCmdText(client, L"ERR Not enough or invalid credits");
				return;
			}

			pub::Player::AdjustCash(client, money);
			base->money -= money;
			base->Save();

			AddLog("NOTICE: Bank withdraw new_balance=%I64d money=%d base=%s charname=%s (%s)",
				base->money, money,
				wstos(base->basename).c_str(),
				wstos(charname).c_str(),
				wstos(HkGetAccountID(HkGetAccountByCharname(charname))).c_str());

			PrintUserCmdText(client, L"OK %u credits withdrawn", money);
		}
		else if (cmd == L"deposit")
		{
			int iCurrMoney;
			pub::Player::InspectCash(client, iCurrMoney);

			if (money > iCurrMoney || money < 0)
			{
				PrintUserCmdText(client, L"ERR Not enough or invalid credits");
				return;
			}

			pub::Player::AdjustCash(client, 0 - money);
			base->money += money;
			base->Save();

			AddLog("NOTICE: Bank deposit money=%d new_balance=%I64d base=%s charname=%s (%s)",
				money, base->money,
				wstos(base->basename).c_str(),
				wstos(charname).c_str(),
				wstos(HkGetAccountID(HkGetAccountByCharname(charname))).c_str());

			PrintUserCmdText(client, L"OK %u credits deposited", money);
		}
		else if (cmd == L"status")
		{
			PrintUserCmdText(client, L"OK current balance %I64d credits", base->money);
		}
		else
		{
			PrintUserCmdText(client, L"ERR Invalid parameters");
			PrintUserCmdText(client, L"/bank [deposit|withdraw|status] [credits]");
		}
	}

	static void ShowShopStatus(uint client, PlayerBase *base, wstring substring, int page)
	{
		int matchingItems = 0;
		for (map<UINT, MARKET_ITEM>::iterator i = base->market_items.begin(); i != base->market_items.end(); ++i)
		{
			const GoodInfo *gi = GoodList::find_by_id(i->first);
			if (!gi)
				continue;

			wstring name = HkGetWStringFromIDS(gi->iIDSName);
			if (ToLower(name).find(substring) != std::wstring::npos) {
				matchingItems++;
			}
		}

		int pages = (matchingItems / 40) + 1;
		if (page > pages)
			page = pages;
		else if (page < 1)
			page = 1;

		wchar_t buf[1000];
		_snwprintf(buf, sizeof(buf), L"Shop Management : Page %d/%d", page, pages);
		wstring title = buf;

		int start_item = ((page - 1) * 40) + 1;
		int end_item = page * 40;

		wstring status = L"<RDL><PUSH/>";
		status += L"<TEXT>Available commands:</TEXT><PARA/>";
		if (clients[client].admin)
		{
			status += L"<TEXT>  /shop price [item] [price] [min stock] [max stock]</TEXT><PARA/>";
			status += L"<TEXT>  /shop remove [item]</TEXT><PARA/>";
		}
		status += L"<TEXT>  /shop [page]</TEXT><PARA/><TEXT>  /shop filter [substring] [page]</TEXT><PARA/><PARA/>";

		status += L"<TEXT>Stock:</TEXT><PARA/>";
		int item = 1;
		int globalItem = 1;

		for (map<UINT, MARKET_ITEM>::iterator i = base->market_items.begin(); i != base->market_items.end(); ++i, globalItem++)
		{
			if (item > end_item)
				break;

			const GoodInfo *gi = GoodList::find_by_id(i->first);
			if (!gi) {
				item++;
				continue;
			}

			wstring name = HkGetWStringFromIDS(gi->iIDSName);
			if (ToLower(name).find(substring) != std::wstring::npos) {
				if (item < start_item) {
					item++;
					continue;
				}
				wchar_t buf[1000];
				_snwprintf(buf, sizeof(buf), L"<TEXT>  %02u:  %ux %s %0.0f credits stock: %u min %u max</TEXT><PARA/>",
					globalItem, i->second.quantity, HtmlEncode(name).c_str(),
					i->second.price, i->second.min_stock, i->second.max_stock);
				status += buf;
				item++;
			}
		}
		status += L"<POP/></RDL>";

		HkChangeIDSString(client, 500000, title);
		HkChangeIDSString(client, 500001, status);

		FmtStr caption(0, 0);
		caption.begin_mad_lib(500000);
		caption.end_mad_lib();

		FmtStr message(0, 0);
		message.begin_mad_lib(500001);
		message.end_mad_lib();

		pub::Player::PopUpDialog(client, caption, message, POPUPDIALOG_BUTTONS_CENTER_OK);
	}

	void Shop(uint client, const wstring &args)
	{
		// Check that this player is in a player controlled base
		PlayerBase *base = GetPlayerBaseForClient(client);
		if (!base)
		{
			PrintUserCmdText(client, L"ERR Not in player base");
			return;
		}

		const wstring &cmd = GetParam(args, ' ', 1);
		if (!clients[client].admin && (!clients[client].viewshop || (cmd == L"price" || cmd == L"remove")))
		{
			PrintUserCmdText(client, L"ERROR: Access denied");
			return;
		}

		if (cmd == L"price")
		{
			int item = ToInt(GetParam(args, ' ', 2));
			int money = ToInt(GetParam(args, ' ', 3));
			int min_stock = ToInt(GetParam(args, ' ', 4));
			int max_stock = ToInt(GetParam(args, ' ', 5));

			if (money < 1 || money > 1000000000)
			{
				PrintUserCmdText(client, L"ERR Price not valid");
				return;
			}

			int curr_item = 1;
			for (map<UINT, MARKET_ITEM>::iterator i = base->market_items.begin(); i != base->market_items.end(); ++i, curr_item++)
			{
				if (curr_item == item)
				{
					i->second.price = (float)money;
					i->second.min_stock = min_stock;
					i->second.max_stock = max_stock;
					SendMarketGoodUpdated(base, i->first, i->second);
					base->Save();

					int page = ((curr_item + 39) / 40);
					ShowShopStatus(client, base, L"", page);
					PrintUserCmdText(client, L"OK");
					return;
				}
			}
			PrintUserCmdText(client, L"ERR Commodity does not exist");
		}
		else if (cmd == L"remove")
		{
			int item = ToInt(GetParam(args, ' ', 2));

			int curr_item = 1;
			for (map<UINT, MARKET_ITEM>::iterator i = base->market_items.begin(); i != base->market_items.end(); ++i, curr_item++)
			{
				if (curr_item == item)
				{
					i->second.price = 0;
					i->second.quantity = 0;
					i->second.min_stock = 0;
					i->second.max_stock = 0;
					SendMarketGoodUpdated(base, i->first, i->second);
					base->market_items.erase(i->first);
					base->Save();

					int page = ((curr_item + 39) / 40);
					ShowShopStatus(client, base, L"", page);
					PrintUserCmdText(client, L"OK");
					return;
				}
			}
			PrintUserCmdText(client, L"ERR Commodity does not exist");
		}
		else if (cmd == L"filter")
		{
			wstring substring = GetParam(args, ' ', 2);
			int page = ToInt(GetParam(args, ' ', 3));
			ShowShopStatus(client, base, ToLower(substring), page);
			PrintUserCmdText(client, L"OK");
		}
		else
		{
			int page = ToInt(GetParam(args, ' ', 1));
			ShowShopStatus(client, base, L"", page);
			PrintUserCmdText(client, L"OK");
		}
	}

	void BaseDeploy(uint client, const wstring &args)
	{
		if (set_holiday_mode)
		{
			PrintUserCmdText(client, L"ERR Cannot create bases when holiday mode is active");
			return;
		}


		// Abort processing if this is not a "heavy lifter"
		uint shiparch;
		pub::Player::GetShipID(client, shiparch);
		if (set_construction_shiparch != 0 && shiparch != set_construction_shiparch)
		{
			PrintUserCmdText(client, L"ERR Need construction ship");
			return;
		}

		uint ship;
		pub::Player::GetShip(client, ship);
		if (!ship)
		{
			PrintUserCmdText(client, L"ERR Not in space");
			return;
		}

		// If the ship is moving, abort the processing.
		Vector dir1;
		Vector dir2;
		pub::SpaceObj::GetMotion(ship, dir1, dir2);
		if (dir1.x > 5 || dir1.y > 5 || dir1.z > 5)
		{
			PrintUserCmdText(client, L"ERR Ship is moving");
			return;
		}

		wstring password = GetParam(args, ' ', 2);
		if (!password.length())
		{
			PrintUserCmdText(client, L"ERR No password");
			PrintUserCmdText(client, L"Usage: /base deploy <password> <name>");
			return;
		}
		wstring basename = GetParamToEnd(args, ' ', 3);
		if (!basename.length())
		{
			PrintUserCmdText(client, L"ERR No base name");
			PrintUserCmdText(client, L"Usage: /base deploy <password> <name>");
			return;
		}

		// Check for conflicting base name
		if (GetPlayerBase(CreateID(PlayerBase::CreateBaseNickname(wstos(basename)).c_str())))
		{
			PrintUserCmdText(client, L"ERR Base name already exists");
			return;
		}

		// Check that the ship has the requires commodities.
		int hold_size;
		list<CARGO_INFO> cargo;
		HkEnumCargo((const wchar_t*)Players.GetActiveCharacterName(client), cargo, hold_size);
		for (map<uint, uint>::iterator i = construction_items.begin(); i != construction_items.end(); ++i)
		{
			bool material_available = false;
			uint good = i->first;
			uint quantity = i->second;
			for (list<CARGO_INFO>::iterator ci = cargo.begin(); ci != cargo.end(); ++ci)
			{
				if (ci->iArchID == good && ci->iCount >= (int)quantity)
				{
					material_available = true;
					pub::Player::RemoveCargo(client, ci->iID, quantity);
				}
			}
			if (material_available == false)
			{
				PrintUserCmdText(client, L"ERR Construction failed due to insufficient raw material.");
				for (i = construction_items.begin(); i != construction_items.end(); ++i)
				{
					const GoodInfo *gi = GoodList::find_by_id(i->first);
					if (gi)
					{
						PrintUserCmdText(client, L"|  %ux %s", i->second, HkGetWStringFromIDS(gi->iIDSName).c_str());
					}
				}
				return;
			}
		}

		wstring charname = (const wchar_t*)Players.GetActiveCharacterName(client);
		AddLog("NOTICE: Base created %s by %s (%s)",
			wstos(basename).c_str(),
			wstos(charname).c_str(),
			wstos(HkGetAccountID(HkGetAccountByCharname(charname))).c_str());

		PlayerBase *newbase = new PlayerBase(client, password, basename);
		player_bases[newbase->base] = newbase;
		newbase->basetype = "legacy";
		newbase->basesolar = "legacy";
		newbase->baseloadout = "legacy";
		newbase->defense_mode = 1;

		for (map<string, ARCHTYPE_STRUCT>::iterator iter = mapArchs.begin(); iter != mapArchs.end(); iter++)
		{

			ARCHTYPE_STRUCT &thearch = iter->second;
			if (iter->first == newbase->basetype)
			{
				newbase->invulnerable = thearch.invulnerable;
				newbase->logic = thearch.logic;
			}
		}
		newbase->Spawn();
		newbase->Save();

		PrintUserCmdText(client, L"OK: Base deployed");
		PrintUserCmdText(client, L"Default administration password is %s", password.c_str());
	}
}
