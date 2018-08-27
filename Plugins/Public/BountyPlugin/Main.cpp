// Bounty Hunting Plugin
// By Laz with the idea and code snippets taken from RawRawRDinosaur
// Functions for rewriting cfg files were from him - credit where credit is due.
// Created July 11th (Birthday :D) 2018
//
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Includes
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Main.h"

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
		if (set_scCfgFile.length()>0)
		{
			LoadSettings();
		}
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
// Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool bPluginEnabled = false; // Plugin Active
bool bAnnounceBountyClaim = false; // Announce when someone with a bounty on their head is killed.
bool bAnnounceBountyExist = false; // Inform someone when a bounty is placed on their head.
bool bAnnounceBountyExistHunters = false; // Inform the hunters when they log that the bounty hunting plugin is a thing (quick, "Check the /bounty list" message)
bool bAnnounceNewBounties = false; // Announce to all IDs eligible when a new bounty is created.
uint iNumberOfDaysPerBounty = 30; // How long will each bounty be valid for?
uint iMaxBountyAmount = 50000000; // Max amount you can bounty someone for.
uint iMinBountyAmount = 5000000; // Min amount you can bounty someone for.
int iProcessingFee = 2000000; // Default amount of money we take away.
int iExportMode = -1; // By default, we do not export data.

string scFilePath, scHTMLPath, scJSONPath;

struct BOUNTYINFO
{
	wstring character; // Who is the bounty against?
	wstring issuer; // Who issued it in the first place?
	uint bountyAmount; // How much for?
	uint claimAmount; // How many times may it be claimed?
	uint claimedAmount; // How many times has it been claimed?
	uint timeUntilDecay; // How many days until this bounty runs it's course?
	string reason; // Make sure an inRP reason is provided for the bounty.

	bool operator == (const BOUNTYINFO& rm) const // This allows us to compare structs
	{
		return (rm.character == character
			&& rm.issuer == issuer
			&& rm.bountyAmount == bountyAmount
			&& rm.claimAmount == claimAmount
			&& rm.claimedAmount == claimedAmount
			&& rm.timeUntilDecay == timeUntilDecay
			&& rm.reason == reason
			);
	}
};

list<uint> allowedIDs; // A list of IDs that can claim bounties
list<BOUNTYINFO> bountyList; // A list of the currently active bounties
list<uint> lstBannedSystems; // Prevent people from making bounty claims/posting bounties in certain systems

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rewriting Config
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

vector<string> bountyVector; // This is where we will store lines of the cfg while we make our changes

void loadfile(const string& sfile)
{
	ifstream tmpCfg(sfile); // Load in the file we specify
	string sTemp; // Temporary string
	if (tmpCfg.fail()) // If we fail to load the cfg return gracefully
		return;

	while (getline(tmpCfg, sTemp)) // Read through the config line by line
	{
		if (!sTemp.empty()) // Provided it's not empty
			bountyVector.emplace_back(sTemp); // Populate the vector with it's contents
	}
	tmpCfg.close(); // We're done, discard it.

}

vector<string> RemoveLineFromVector(vector<string>toRemove, vector<string> vectorConfig)
{
	for (int i = 0; i < vectorConfig.size(); ++i)
	{
		if (toRemove[0] == vectorConfig[i] && toRemove[1] == vectorConfig[i + 1] && toRemove[2] == vectorConfig[i + 2]
			&& toRemove[3] == vectorConfig[i + 3] && toRemove[4] == vectorConfig[i + 4] && toRemove[5] == vectorConfig[i + 5]
			&& toRemove[6] == vectorConfig[i + 6] && toRemove[7] == vectorConfig[i + 7]) 
			// This is HORRIBLE, but I am still looking for a better way. Will do until I find a better way.
		{
			vectorConfig.erase(vectorConfig.begin() + i + 7);
			vectorConfig.erase(vectorConfig.begin() + i + 6);
			vectorConfig.erase(vectorConfig.begin() + i + 5);
			vectorConfig.erase(vectorConfig.begin() + i + 4);
			vectorConfig.erase(vectorConfig.begin() + i + 3);
			vectorConfig.erase(vectorConfig.begin() + i + 2);
			vectorConfig.erase(vectorConfig.begin() + i + 1);
			vectorConfig.erase(vectorConfig.begin() + i);
			break;
		}
	}
	return vectorConfig;
}

void RestoreFileContents(const string& toOpen, vector<string> vectorFile)
{
	ofstream stream;
	stream.open(toOpen, ios::out | ios::trunc);
	stream << "; Do not manually edit this file unless it is strictly required." << endl;
	stream << "; Editing this file could cause corruption and break the plugin." << endl;
	for (auto& i : vectorFile)
	{
		stream << i << endl;
	}	
	stream.close();
}

void RemoveBountyFromCfg(BOUNTYINFO BI)
{
	loadfile(scFilePath); // Load the Vector with the list of bounties
	vector<string> item;
	item.emplace_back("[Bounty]");
	item.emplace_back("bounty = " + wstos(BI.character));
	item.emplace_back("amount = " + itos(BI.bountyAmount));
	item.emplace_back("claim = " + itos(BI.claimAmount));
	item.emplace_back("claimed = " + itos(BI.claimedAmount));
	item.emplace_back("issuer = " + wstos(BI.issuer));
	item.emplace_back("decay = " + itos(BI.timeUntilDecay));
	item.emplace_back("reason = " + BI.reason);
	bountyVector = RemoveLineFromVector(item, bountyVector); // Find the line we want to remove and remove it from the vector
	RestoreFileContents(scFilePath, bountyVector); // Overwrite 
}

bool AppendBountyInCfg(BOUNTYINFO BI, bool decDuration, bool incClaim, int bountyIncrease, wstring newCharacter, wstring newIssuer)
{
	RemoveBountyFromCfg(BI); // If we are writing a new line, we need to delete the old one!
	const string CfgLocation = scFilePath; // Our config? 
	ofstream tmpCfg; // New Stream

	tmpCfg.open(CfgLocation, ios::out | ios::app);
	if (!tmpCfg.is_open())
		return false;

	if (decDuration)
		BI.timeUntilDecay--;

	if (incClaim)
		BI.claimedAmount++;

	if (bountyIncrease != 0)
		BI.bountyAmount = BI.bountyAmount + bountyIncrease;

	if (!newCharacter.empty())
		BI.character = newCharacter;

	if (!newIssuer.empty())
		BI.character = newIssuer;

	tmpCfg << "[Bounty]" << endl;
	tmpCfg << "bounty = " << wstos(BI.character) << endl;
	tmpCfg << "amount = " << itos(BI.bountyAmount) << endl;
	tmpCfg << "claim = " << itos(BI.claimAmount) << endl;
	tmpCfg << "claimed = " << itos(BI.claimedAmount) << endl;
	tmpCfg << "issuer = " << wstos(BI.issuer) << endl;
	tmpCfg << "decay = " << itos(BI.timeUntilDecay) << endl;
	tmpCfg << "reason = " << BI.reason << endl;

	tmpCfg.close();
	bountyVector.clear(); // If we don't clear this, we'll duplicate the entire config
	return true;
}

void LoadBounties()
{
	INI_Reader ini;
	int iActiveBounties = 0; // We want to print to the console how many active bounties there are on each server restart
	if (ini.open(scFilePath.c_str(), false)) // Our Bountylist file, rather than our config file.
	{
		while (ini.read_header())
		{
			if (ini.is_header("Bounty"))
			{
				BOUNTYINFO BI;
				while (ini.read_value())
				{
					if (ini.is_value("bounty"))
					{
						BI.character = stows(ini.get_value_string()); // Store the character name of the guy with the big bounty on his head
					}
					else if (ini.is_value("amount"))
					{
						BI.bountyAmount = strtoul(ini.get_value_string(), nullptr, 0); // How big is that bounty on his head
					}
					else if (ini.is_value("claim"))
					{
						BI.claimAmount = strtoul(ini.get_value_string(), nullptr, 0); // How many times can someone claim the big bounty on his head
					}
					else if (ini.is_value("claimed"))
					{
						BI.claimedAmount = strtoul(ini.get_value_string(), nullptr, 0); // How many times has someone claimed the bounty on his head
					}
					else if (ini.is_value("issuer"))
					{
						BI.issuer = stows(ini.get_value_string()); // Who put that big bounty on his head
					}
					else if (ini.is_value("decay"))
					{
						BI.timeUntilDecay = strtoul(ini.get_value_string(), nullptr, 0); // How many days until the guy is rid of the big bounty on his head
					}
					else if (ini.is_value("reason"))
					{
						BI.reason = ini.get_value_string(); // For what reason could this guy have gained that big bounty on his head
					}
				}
				bountyList.emplace_back(BI); // Store it all in the list for later huehuehue
				iActiveBounties++;
			}
		}
	}
	ini.close();
	ConPrint(L"BOUNTY: %u active bounties loaded\n", iActiveBounties); // List active bounties
}

void DecrementBountyInCfg(list<BOUNTYINFO> activeBounties)
{
	bountyList.clear();
	foreach(activeBounties, BOUNTYINFO, iter)
		AppendBountyInCfg(*iter, true, false, 0, L"", L"");
	LoadBounties();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Loading Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	bountyList.clear();

	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof (szCurDir), szCurDir);
	const string scPluginCfgFile = string(szCurDir) + R"(\flhook_plugins\laz_bounty.cfg)"; // Our config
	scFilePath = string(szCurDir) + R"(\flhook_plugins\laz_bountylist.cfg)"; // Our store of bounties;

	bPluginEnabled = IniGetB(scPluginCfgFile, "Config", "enabled", bPluginEnabled); // Check the config to see if we enable it
	bAnnounceBountyClaim = IniGetB(scPluginCfgFile, "Config", "announceClaim", bAnnounceBountyClaim); // Check the config to see if we enable it
	bAnnounceBountyExist = IniGetB(scPluginCfgFile, "Config", "announceBounty", bAnnounceBountyExist); // Check the config to see if we enable it
	bAnnounceBountyExistHunters = IniGetB(scPluginCfgFile, "Config", "announceHunters", bAnnounceBountyExistHunters); // Check the config to see if we enable it
	bAnnounceNewBounties = IniGetB(scPluginCfgFile, "Config", "announceNewBounties", bAnnounceNewBounties); // Check the config to see if we enable it
	iNumberOfDaysPerBounty = IniGetI(scPluginCfgFile, "Config", "bountyLifespan", iNumberOfDaysPerBounty) + 1; // How many days will each bounty last?
	iMaxBountyAmount = IniGetI(scPluginCfgFile, "Config", "maxBounty", iMaxBountyAmount); // Max amount of money that can be placed
	iMinBountyAmount = IniGetI(scPluginCfgFile, "Config", "minBounty", iMinBountyAmount); // Min amount of money that can be placed
	iProcessingFee = IniGetI(scPluginCfgFile, "Config", "fee", iProcessingFee); // How much money do we take away as part of a Processing fee?
	iExportMode = IniGetI(scPluginCfgFile, "Config", "export", -1);

	scHTMLPath = IniGetS(scPluginCfgFile, "Config", "html", string(szCurDir) + R"(\Exports\BountyExports.html)"); // Default export location for the html file
	scJSONPath = IniGetS(scPluginCfgFile, "Config", "json", string(szCurDir) + R"(\Exports\BountyExports.json)"); // Default export location for the json file

	INI_Reader ini;
	if (ini.open(scPluginCfgFile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("Banned Systems"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("system"))
					{
						lstBannedSystems.emplace_back(CreateID(ini.get_value_string(0))); // A list of systems where the bounties will be ignored upon kill.
					}
				}
			}

			else if (ini.is_header("Allowed IDs"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("ID"))
					{
						allowedIDs.emplace_back(CreateID(ini.get_value_string(0))); // Create a uint from the specified string and put in the allowed IDs list
					}
				}
			}
		}
	}
	ini.close();
	LoadBounties();
	foreach(bountyList, BOUNTYINFO, iter)
	{
		if (iter->timeUntilDecay <= 0) // The bounty has expired
		{
			RemoveBountyFromCfg(*iter); // Remove old bounty!
			HK_ERROR err; // Error
			if ((err = HkAddCash(iter->issuer, iter->bountyAmount * (iter->claimAmount - iter->claimedAmount))) != HKE_OK)
			{ // We need to add back any cash they were owed if the bounty failed
				const wstring character = iter->issuer; // Who issued the bounty
				const wstring cash = stows(itos(iter->bountyAmount * (iter->claimAmount - iter->claimedAmount))); // How much
				AddLog(wstos(L"BOUNTY: Error giving " + character + L" " + cash + // For some reason this errors if I create strings, so I convert to wstring and then back.
					L" after bounty timeout. Please manually return this character's credit.").c_str());
				MailSend(iter->issuer, "-mail.ini", L"There was an error in the automatic bounty repayment system. Please contact admin for cash return.");
				continue;
			}
			MailSend(iter->issuer, "-mail.ini", L"Your bounty listing on " + iter->character + L" has expired. You have been returned the remaining "
				+ ToMoneyStr(iter->bountyAmount * (iter->claimAmount - iter->claimedAmount)) + L" credits. Note: " + ToMoneyStr(iProcessingFee)
				+ L" credits has been deducted from your account to pay for the listing.");
		}
	}

	DecrementBountyInCfg(bountyList); // Lower the days until expire on all bounties
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UserCmd_BountyNew(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginEnabled)
	{
		PrintUserCmdText(iClientID, L"The Bounty Plugin is disabled.");
		return true;
	}

	uint iPlayerSystem; // Get the internal ID of the current system
	pub::Player::GetSystem(iClientID, iPlayerSystem);

	if (find(lstBannedSystems.begin(), lstBannedSystems.end(), iPlayerSystem) != lstBannedSystems.end()) // Are they currently in a banned system, like Bastille?
	{
		PrintUserCmdText(iClientID, L"ERR: You cannot create a bounty in this system."); // If so conclude here
		return true;
	}

	// NOTE: You can stack multiple bounties on the same person. This is intended!

	const wstring wscTarget = GetParam(wscParam, L' ', 0);
	const int iClaimable = ToInt(GetParam(wscParam, L' ', 1));
	const int iCash = ToInt(GetParam(wscParam, L' ', 2));
	const wstring wscReason = GetParamToEnd(wscParam, L' ', 3);

	if(wscTarget.empty()) // Make sure they specify someone
	{
		PrintUserCmdText(iClientID, L"ERR: Invalid Target Name");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	if (iClaimable <= 0) // They cannot set a claim to be less than or equal to 0 times.
	{
		PrintUserCmdText(iClientID, L"ERR: Invalid Claim Amount");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	if(iCash <= 0) // Make sure they actually put in a valid amount
	{
		PrintUserCmdText(iClientID, L"ERR: Bounty amount is zero or invalid.");
		return true;
	}

	int iCurrMoney;
	pub::Player::InspectCash(iClientID, iCurrMoney);
	const int toDeduct = (iCash * iClaimable) + iProcessingFee;

	if (iCurrMoney < toDeduct) // Do they have enough money to do this?
	{
		PrintUserCmdText(iClientID, L"ERR: You do not have enough money to reserve for the bounty.");
		return true;
	}

	if (iCash < iMinBountyAmount) // Not enough money?
	{
		PrintUserCmdText(iClientID, L"ERR: Invalid Bounty Amount - %s is the minimum amount of credits permitted.", ToMoneyStr(iMinBountyAmount).c_str());
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	if (iCash > iMaxBountyAmount) // Too much money?
	{
		PrintUserCmdText(iClientID, L"ERR: Invalid Bounty Amount - %s is the maximum amount of credits permitted.", ToMoneyStr(iMinBountyAmount).c_str());
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	if(wscReason.empty()) // Have they provided a reason?
	{
		PrintUserCmdText(iClientID, L"ERR: You didn't supply an inRP reason for the bounty. This is required.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	if(HkGetAccountByCharname(wscTarget) == nullptr) // Does the player exist?
	{
		PrintUserCmdText(iClientID, L"ERR: Player does not exist.");
		return true;
	}

	wstring wscCharacterName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID)); // What is the name of the character setting the bounty

	BOUNTYINFO BI; // Create a new bounty
	BI.character = wscTarget; // Target
	BI.bountyAmount = iCash; // How much will the target be payed
	BI.claimAmount = iClaimable; // How many times will it be claimable
	BI.issuer = wscCharacterName; // Who sent it
	BI.claimedAmount = 0; // Default claimed 0 times.
	BI.reason = wstos(wscReason); // What reason
	BI.timeUntilDecay = iNumberOfDaysPerBounty; // Default

	HK_ERROR err;
	if ((err = HkAddCash(wscCharacterName, 0 - toDeduct)) != HKE_OK) // Attempt to take money
	{
		PrintUserCmdText(iClientID, L"ERR: Unable to create bounty. Error Message: " + HkErrGetText(err)); // If problem, let them know.
		return true;
	}

	if(!AppendBountyInCfg(BI, false, false, 0, L"", L"")) // Add the bounty to the cfg
	{
		PrintUserCmdText(iClientID, L"There was an unknown error while creating the bounty. Please try again."); // The file was probably being opened by someone else
		return true;
	}

	bountyList.emplace_back(BI); // Add to the current list of bounties.

	PrintUserCmdText(iClientID, L"You have successfully submitted a new bounty with the following information:");
	PrintUserCmdText(iClientID, L"Target: %s", wscTarget.c_str());
	PrintUserCmdText(iClientID, L"Payout per kill: %s", ToMoneyStr(iCash).c_str());
	PrintUserCmdText(iClientID, L"Number of Kills permitted: %s", stows(itos(iClaimable)).c_str());
	PrintUserCmdText(iClientID, L"Reason: %s", wscReason.c_str());
	PrintUserCmdText(iClientID, L"Days Until Bounty Expire: %s", stows(itos(iNumberOfDaysPerBounty - 1)).c_str()); // 31 by default
	PrintUserCmdText(iClientID, L"Amount Of Credits Deducted From Account (will be returned if bounty is not completed): %s", ToMoneyStr(toDeduct).c_str());
	PrintUserCmdText(iClientID, L"Credits Deducted For Listing Fee: %s", ToMoneyStr(iProcessingFee).c_str());

	if (bAnnounceNewBounties) // Do we have Sirius wide announcements enabled
	{
		const wstring bountyMsg = L"BOUNTY: A bounty has been placed on " + BI.character + L" for " +
			ToMoneyStr(BI.bountyAmount) + L" credits by " + wscCharacterName; // Create the message we will send to everyone
		struct PlayerData *pd = 0; // All players
		while (pd = Players.traverse_active(pd)) // Loop over all online players
		{
			if (pd->iOnlineID == iClientID) // If the ID is the person that sent it, they don't need to be notified
				continue;
			if (pd->iOnlineID == HkGetClientIdFromCharname(BI.character))
				continue;
			PrintUserCmdText(pd->iOnlineID, bountyMsg); // Send them text about how there is now a new bounty available!
		}
	}

	const uint targetClientID = HkGetClientIdFromCharname(BI.character); // Get the client ID of target, returns -1 if they are offline.
	const wstring claimMsg = wscCharacterName + L" has placed bounty on you " + ToMoneyStr(BI.bountyAmount) + L" credits!";
	if (targetClientID != -1 && !HkIsInCharSelectMenu(targetClientID)) // If the user is online, we send them a message saying the bounty has been posted
	{
		PrintUserCmdText(targetClientID, L"%s", claimMsg.c_str()); // Send that message
	}

	else
	{
		MailSend(BI.character, "-mail.ini", claimMsg); // If they are offline, we send the message via mail. They can check it when they log in.
	}

	return true;
}

bool UserCmd_BountyHelp(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginEnabled)
	{
		PrintUserCmdText(iClientID, L"The Bounty Plugin is disabled.");
		return true;
	}

	const wstring bountyHelp = L"<RDL><PUSH/>" // You know me, I love those help menus!

		L"<TRA bold=\"true\"/><TEXT>/bounty new</TEXT><TRA bold=\"false\"/><PARA/>"
		L"<TEXT>Setup a new bounty. Run it without any parameters to see it's usage.</TEXT><PARA/><PARA/>"

		L"<TRA bold=\"true\"/><TEXT>/bounty help</TEXT><TRA bold=\"false\"/><PARA/>"
		L"<TEXT>Show this menu</TEXT><PARA/><PARA/>"

		L"<TRA bold=\"true\"/><TEXT>/bounty add</TEXT><TRA bold=\"false\"/><PARA/>"
		L"<TEXT>Add money to an existing bounty. Run it without any parameters to see it's usage.</TEXT><PARA/><PARA/>"

		L"<TRA bold=\"true\"/><TEXT>/bounty remove</TEXT><TRA bold=\"false\"/><PARA/>"
		L"<TEXT>Remove an existing bounty. Run it without any parameters to see it's usage.</TEXT><PARA/><PARA/>"

		L"<TRA bold=\"true\"/><TEXT>/bounty list</TEXT><TRA bold=\"false\"/><PARA/>"
		L"<TEXT>Get a list of active bounties.</TEXT><PARA/><PARA/>"

		L"<TRA bold=\"true\"/><TEXT>/bounty me</TEXT><TRA bold=\"false\"/><PARA/>"
		L"<TEXT>Is there a bounty on YOUR head? Find out now!</TEXT><PARA/><PARA/>"

		L"<POP/></RDL>";

	HkChangeIDSString(iClientID, 500000, L"Bounty Plugin Help Menu"); // I really love those help menus
	HkChangeIDSString(iClientID, 500001, bountyHelp); // Change IDS value to the help menu we want

	FmtStr caption(0, 0);
	caption.begin_mad_lib(500000);
	caption.end_mad_lib();

	FmtStr message(0, 0);
	message.begin_mad_lib(500001);
	message.end_mad_lib();

	pub::Player::PopUpDialog(iClientID, caption, message, POPUPDIALOG_BUTTONS_CENTER_OK); // Create a helpmenu. (Did I mention I love help menus)

	PrintUserCmdText(iClientID, L"OK"); // Verify everything went to plan

	return true;
}

bool UserCmd_BountyAdd(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginEnabled)
	{
		PrintUserCmdText(iClientID, L"The Bounty Plugin is disabled.");
		return true;
	}

	const wstring wscPlayerName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID));
	const wstring wscTarget = GetParam(wscCmd, L' ', 2);
	const int iCash = ToInt(GetParam(wscCmd, L' ', 3));

	if (wscTarget.empty()) // Make sure they specify someone
	{
		PrintUserCmdText(iClientID, L"ERR: Invalid Target Name");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	if (iCash <= 0)
	{
		PrintUserCmdText(iClientID, L"You have to actually pay for the increase in bounty, cheapskate.");
		return true;
	}

	bool foundIssuer, foundTarget; // We will set these to true as we find them
	BOUNTYINFO BI; // Empty Bountyinfo, will populate if we find a valid match.

	foreach(bountyList, BOUNTYINFO, iter)
	{
		if (iter->issuer != wscPlayerName)
			continue;
		foundIssuer = true;
		if (iter->character != wscTarget)
			continue;
		foundTarget = true;
		BI = *iter;
		break;
	}

	if (!foundIssuer)
	{
		PrintUserCmdText(iClientID, L"You currently do not have any bounties issued in your name.");
		return true;
	}

	if (!foundTarget)
	{
		PrintUserCmdText(iClientID, L"The target specified is not one of your current contracts.");
		return true;
	}

	int iCurrMoney;
	pub::Player::InspectCash(iClientID, iCurrMoney);
	if (iCash * (BI.claimAmount - BI.claimedAmount) > iCurrMoney) // Make sure they have enough money for the remaining bounties
	{
		PrintUserCmdText(iClientID, L"You don't have enough money to increase the bounty!");
		return true;
	}

	if (BI.bountyAmount + iCash > iMaxBountyAmount) // Make sure the increase wouldn't go over the imposed limit
	{
		PrintUserCmdText(iClientID, L"Increasing the bounty by this much would go over the maximum listing cap!");
		return true;
	}

	HK_ERROR err;
	if ((err = HkAddCash(wscPlayerName, 0 - iCash)) != HKE_OK) // Attempt to take money
	{
		PrintUserCmdText(iClientID, L"ERR: Unable to increase bounty ammount. Error Message: " + HkErrGetText(err)); // If problem, let them know.
		return true;
	}

	bountyList.remove(BI); // Remove the old bounty
	if(!AppendBountyInCfg(BI, false, false, iCash, L"", L""))
	{
		PrintUserCmdText(iClientID, L"There was an unknown error while increasing the bounty. Please try again."); // The file was probably being opened by someone else
		return true;
	}

	
	BI.bountyAmount = BI.bountyAmount + iCash;
	bountyList.emplace_back(BI);
	PrintUserCmdText(iClientID, L"You have successfully increased the target bounty to a total of %u for the remaining %u bounties.", BI.bountyAmount, BI.claimAmount - BI.claimedAmount);
	return true;
}

bool UserCmd_BountyRemove(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginEnabled)
	{
		PrintUserCmdText(iClientID, L"The Bounty Plugin is disabled.");
		return true;
	}

	const wstring wscCharname = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID));
	const wstring wscTarget = GetParam(wscCmd, L' ', 2);
	bool foundIssuer, foundTarget;
	BOUNTYINFO BI;

	foreach(bountyList, BOUNTYINFO, iter)
	{
		if (iter->issuer != wscCharname)
			continue;
		foundIssuer = true;
		if (iter->character != wscTarget)
			continue;
		foundTarget = true;
		BI = *iter;
	}

	if(!foundIssuer)
	{
		PrintUserCmdText(iClientID, L"You currently do not have any bounties issued in your name.");
		return true;
	}

	if(!foundTarget)
	{
		PrintUserCmdText(iClientID, L"The target specified is not one of your current contracts.");
		return true;
	}

	int iCurrMoney;
	pub::Player::InspectCash(iClientID, iCurrMoney);

	HK_ERROR err;
	if ((err = HkAddCash(wscCharname, 0 - BI.bountyAmount * (BI.claimAmount - BI.claimedAmount))) != HKE_OK) // Attempt to take money
	{
		PrintUserCmdText(iClientID, L"ERR: Unable to remove bounty with refund. Error Message: " + HkErrGetText(err)); // If problem, let them know.
		return true;
	}

	bountyList.remove(BI);
	RemoveBountyFromCfg(BI);
	PrintUserCmdText(iClientID, L"You have successfully removed your listing against %s.", BI.character);
	PrintUserCmdText(iClientID, L"You have been refunded the remaining %s credits, with the deduction of the %s credit processing fee.",
		ToMoneyStr(BI.bountyAmount * (BI.claimAmount - BI.claimedAmount)).c_str(), ToMoneyStr(iProcessingFee).c_str());

	return true;
}

bool UserCmd_BountyList(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginEnabled)
	{
		PrintUserCmdText(iClientID, L"The Bounty Plugin is disabled.");
		return true;
	}

	if (bountyList.empty())
	{
		PrintUserCmdText(iClientID, L"There appears to be no bounties currently available. Create your own with /bounty new !");
		return true;
	}

	int iHoldSize; // Placeholder. We don't use this, but it's required.
	list<CARGO_INFO> lstCargo; // List to store all our cargo items
	HkEnumCargo(reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID)), lstCargo, iHoldSize); // Get all their cargo
	for (auto& i : lstCargo) // Loop over their cargo and equipment
	{
		if (find(allowedIDs.begin(), allowedIDs.end(), i.iArchID) != allowedIDs.end()) // If we find out they are valid bounty hunter
		{
			// Pagination idea taken from PoB Plugin
			// Don't fully understand this.
			int iCurPage = ToInt(GetParam(wscCmd, L' ', 2)); // Get the designated page, 0 if unspecified or words provided.
			int iTotalPages = (bountyList.size() / 10) + 1; // List 10 Bounties Per Page - Too many will hit the 4000 character limit if people give long reasons.

			if (iCurPage > iTotalPages) // If page specified is over the max amount of pages
				iCurPage = iTotalPages; // Set page to last page
			else if (iCurPage < 1) // If page specified is under min amount of pages
				iCurPage = 1; // Set to first page

			wchar_t buf[1000];
			_snwprintf(buf, sizeof(buf), L"Bounty Contracts Available: (Page %d of %d)", iCurPage, iTotalPages);
			const wstring title = buf;

			const int firstBounty = ((iCurPage - 1) * 10) + 1; // First bounty in the list
			const int lastBounty = iCurPage * 10; // Last bounty in the list

			wstring status = L"<RDL><PUSH/><PARA/><PARA/>"; // Start of our content for our helpmenu
			status += L"<TEXT>Switch pages with: /bounty list pagenumber</TEXT><PARA/>";
			status += L"<TEXT>Example: /bounty list 2</TEXT><PARA/><PARA/>";
			status += L"<TEXT>Bounty List:</TEXT><PARA/><PARA/>";

			int iBounty = 0;
			foreach(bountyList, BOUNTYINFO, iter)
			{
				iBounty++;
				if (iBounty < firstBounty)
					continue;
				if (iBounty > lastBounty)
					break;
				status += L"<TEXT>Target: " + iter->character + L"</TEXT><PARA/>";
				status += L"<TEXT>Payout: " + ToMoneyStr(iter->bountyAmount) + L" credits</TEXT><PARA/>";
				status += L"<TEXT>Claimable: " + stows(itos(iter->claimAmount - iter->claimedAmount)) + L" time(s)</TEXT><PARA/>";
				status += L"<TEXT>Issued By: " + iter->issuer + L"</TEXT><PARA/>";
				status += L"<TEXT>Reason: " + stows(iter->reason) + L"</TEXT><PARA/>";
				status += L"<TEXT>Contract Expires In: " + stows(itos(iter->timeUntilDecay - 1)) + L" day(s).</TEXT><PARA/><PARA/>";
			}

			status += L"<POP/></RDL>"; // End of our helpmenu

			HkChangeIDSString(iClientID, 500000, title); // Change IDS string 
			HkChangeIDSString(iClientID, 500001, status); // Change IDS string

			FmtStr caption(0, 0);
			caption.begin_mad_lib(500000);
			caption.end_mad_lib();

			FmtStr message(0, 0);
			message.begin_mad_lib(500001);
			message.end_mad_lib();

			pub::Player::PopUpDialog(iClientID, caption, message, POPUPDIALOG_BUTTONS_CENTER_OK);
		}
		else
			PrintUserCmdText(iClientID, L"You are not currently a registered bounty hunter, and therefore cannot view the listings.");
	}

	return true;
}

// A simple command for checking whether you have a bounty on your head. Useful if the undock alerts are disabled.
bool UserCmd_DoIHaveABounty(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if(!bPluginEnabled)
	{
		PrintUserCmdText(iClientID, L"The Bounty Plugin is disabled.");
		return true;
	}

	wstring wscCharacterName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID)); // Get their character name
	bool wanted = false; // By default they aren't wanted.
	foreach(bountyList, BOUNTYINFO, iter) // Loop over all the bounties and inform them about each bounty on their head
	{
		if(iter->character == wscCharacterName) // Do the names match?
		{
			PrintUserCmdText(iClientID, L"Oh dear, you must have been bad. Looks like %s put a bounty on your head! (%s credits)",  // Inform them someone is after their head!
				iter->issuer.c_str(), ToMoneyStr(iter->bountyAmount).c_str()); // Issuer + How much for
			wanted = true; // Set this to true so we don't give them mixed messages
		}
	}
	if(!wanted)
		PrintUserCmdText(iClientID, L"Good news, you've not angered anyone enough to be given a bounty (yet)!"); // They been a good boy/girl.
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Events
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall ShipDestroyed(DamageList *_dmg, DWORD *ecx, uint iKill)
{
	returncode = DEFAULT_RETURNCODE;
	if (!bPluginEnabled)
		return;

	if (iKill)
	{
		CShip *cship = (CShip*)ecx[4];
		if (cship->is_player()) // Was the ship a player?
		{
			const uint iDestroyedID = cship->GetOwnerPlayer(); // Get the ID of the destroyed ship
			const wstring wscDestroyedName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iDestroyedID)); // Get the character name of the destroyed guy

			uint iDestroyedSystemID; // Empty uint value to populate
			pub::Player::GetSystem(iDestroyedID, iDestroyedSystemID); // We get the ID of the system they are currently in
			if (find(lstBannedSystems.begin(), lstBannedSystems.end(), iDestroyedSystemID) != lstBannedSystems.end()) // If they are currently in a system where we ignore bounty kills
				return; // ignore the kill

			DamageList dmg; // Get information on the last thing that hit the dying target
			try { dmg = *_dmg; }
			catch (...) { return; }
			dmg = ClientInfo[iDestroyedID].dmgLast; // Get information on the last thing to deal damage

			const uint iKillID = HkGetClientIDByShip(dmg.get_inflictor_id()); // Client ID of the thing that did the killing
			const wstring wscKillerName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iKillID)); // Get the character name of the guy to land the last hit

			if (stoi(wstos(HkGetAccountIDByClientID(iKillID))) == -1) // Find out if the last hit was from an AI.
				return;

			if (iKillID == iDestroyedID) // Guy killed himself. Ignore
				return;

			int iHoldSize; // Placeholder for holdsize - required but unused
			list<CARGO_INFO> lstCargo; // A List which we can populate with the killers cargo and equipment
			HkEnumCargo((const wchar_t*)Players.GetActiveCharacterName(iKillID), lstCargo, iHoldSize); // Get all their cargo
			for (auto& i : lstCargo) // Loop over their cargo and equipment
			{
				if (find(allowedIDs.begin(), allowedIDs.end(), i.iArchID) != allowedIDs.end()) // If we find out they are valid bounty hunter
				{
					foreach(bountyList, BOUNTYINFO, iter) // Iterate over the list of bounty targets to see if we killed the right guy
					{
						if (wscDestroyedName == iter->character) // A valid ID has killed a valid target
						{
							BOUNTYINFO BI; // We need to recrete the struct object so it is no longer of type iterator and we can send it to other functions
							BI.character = iter->character; // Character Name
							BI.bountyAmount = iter->bountyAmount; // Amount for kill
							BI.claimAmount = iter->claimAmount; // How many times can it be claimed
							BI.claimedAmount = iter->claimedAmount; // How many times can it be claimed in total
							BI.timeUntilDecay = iter->timeUntilDecay; // How many server restarts will it last?
							BI.issuer = iter->issuer; // Who issued the bounty
							BI.reason = iter->reason; // Why would anyone do such a thing?

							if (BI.claimAmount + 1 >= BI.claimedAmount) // If the amount of claims now equals the total allowed claims
							{
								bountyList.remove(BI); // Remove it from the active bounties
								RemoveBountyFromCfg(BI); // Remove it from the config
							}
							else
							{
								AppendBountyInCfg(BI, false, true, 0, L"", L""); // Increase the amount of times it has been claimed by 1.
							}

							if (bAnnounceBountyClaim) // Do we have Sirius wide announcements enabled
							{
								const wstring bountyMsg = L"BOUNTY: A bounty on " + BI.character + L" has been claimed for " +
									ToMoneyStr(BI.bountyAmount) + L" credits by " + wscKillerName; // Create the message we will send to everyone
								struct PlayerData *pd = 0;
								while (pd = Players.traverse_active(pd)) // Loop over all online players
								{
									PrintUserCmdText(pd->iOnlineID, bountyMsg); // Send them text about how the bounty was claimed
									if (BI.claimedAmount + 1 < BI.claimAmount) // Check if there are any claims left
										PrintUserCmdText(pd->iOnlineID, L"The bounty is still active. Assuming the target will reappear, he can be claimed another %u times!",
											BI.claimAmount - BI.claimedAmount + 1); // If there are we want to inform how many.
								}
							}

							HK_ERROR err;

							if (HkAntiCheat(iKillID) != HKE_OK) // Make sure the anticheat is okay with sending this guy the money he earned.
							{
								PrintUserCmdText(iKillID, L"ERR: Bounty Claim Error"); // If we are not, a simple error message will do.
								AddLog("NOTICE: Possible cheating when claiming a bounty on %s (%s) by %s (%s)", // Log that as possible cheating.
									wstos(wscDestroyedName).c_str(), wstos(HkGetAccountID(HkGetAccountByCharname(wscDestroyedName))).c_str(),
									wstos(wscKillerName).c_str(), wstos(HkGetAccountID(HkGetAccountByCharname(wscKillerName))).c_str());
								return;
							}

							if ((err = HkAddCash(wscKillerName, BI.bountyAmount)) != HKE_OK) // Does the game take issue with giving the poor guy some cash?
							{
								PrintUserCmdText(iKillID, L"ERR: Failed to claim bounty. Error=" + HkErrGetText(err)); // Oh it does. Oh well.
								return;
							}
							HkSaveChar(wscKillerName); // Save the guy now he's got some cash.

							const uint issuerClientID = HkGetClientIdFromCharname(BI.issuer);
							const wstring claimMsg = wscKillerName + L" has claimed the bounty on " + wscDestroyedName + L" for " + ToMoneyStr(BI.bountyAmount) + L" credits.";

							if (issuerClientID != -1 && !HkIsInCharSelectMenu(issuerClientID)) // If the user is online, we send them a message saying the bounty was claimed.
							{
								 PrintUserCmdText(issuerClientID, L"%s", claimMsg.c_str()); // Send that message
								 if (BI.claimedAmount + 1 == BI.claimAmount)
									 PrintUserCmdText(issuerClientID, L"The bounty on %s has now been completed and is no longer active.", BI.character.c_str());
							}

							else
							{
								MailSend(BI.issuer, "-mail.ini", claimMsg); // If they are offline, we send the message via mail. They can check it when they log in.
								if (BI.claimedAmount + 1 == BI.claimAmount)
									MailSend(BI.issuer, "-mail.ini", L"The bounty on " + BI.character + L" has now been completed and is no longer active.");
							}
						}
					}
				}
			}
		}
	}
}

// When someone undocks we have the ability to notify them of their bounty
void __stdcall PlayerLaunch_AFTER(unsigned int iShip, unsigned int iClientID)
{
	if(bAnnounceBountyExist) // If the alert is enabled
	{
		uint iSystemID; // Get their current system
		pub::Player::GetSystem(iClientID, iSystemID);
		const wstring wscCharacterName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID)); // Get their character name from their ID
		if (find(lstBannedSystems.begin(), lstBannedSystems.end(), iSystemID) == lstBannedSystems.end()) // if they are not in a banned system
		{
			foreach(bountyList, BOUNTYINFO, iter) // Loop over all the bounties and inform them about each bounty they have
			{
				if(iter->character == wscCharacterName) // If they have one alert
				{
					PrintUserCmdText(iClientID, L"You currently have a bounty on your head for %s credits, issued by %s!", ToMoneyStr(iter->bountyAmount).c_str(), iter->issuer);
				}
			}
		}
	}

	if(bAnnounceBountyExistHunters)
	{
		uint iSystemID;
		pub::Player::GetSystem(iClientID, iSystemID);
		const wstring wscCharacterName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID));
		if (find(lstBannedSystems.begin(), lstBannedSystems.end(), iSystemID) == lstBannedSystems.end())
		{
			int iHoldSize; // Placeholder for holdsize - required but unused
			list<CARGO_INFO> lstCargo; // A List which we can populate with the hunters cargo and equipment
			HkEnumCargo(reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID)), lstCargo, iHoldSize); // Get all their cargo
			for (auto& i : lstCargo) // Loop over their cargo and equipment
			{
				if (find(allowedIDs.begin(), allowedIDs.end(), i.iArchID) != allowedIDs.end()) // If we find out they are valid bounty hunter
				{
					if(!bountyList.empty()) // If we have a bounty to tell them about.
					{
						PrintUserCmdText(iClientID, L"Attention Bounty Hunters! There are contracts available! View the bounty listings for more information! (/bounty list)");
						break;
					}
				}
			}
		}
	}
}

// When a client renames themselves, we want to be informed of it so we can update the bounty to reflect that.
void Plugin_Communication_CallBack(PLUGIN_MESSAGE msg, void* data)
{
	returncode = DEFAULT_RETURNCODE;

	// Is the data being sent to all plugins applicable to us?
	if (msg == CLIENT_RENAME)
	{
		pair<wstring, wstring>* pRenameInfo = reinterpret_cast<pair<wstring, wstring>*>(data); // Import the old character name, and the new one
		foreach(bountyList, BOUNTYINFO, iter) // Iterate over the bounty list
		{
			BOUNTYINFO BI = *iter; // We will need to use a non interator type
			if(pRenameInfo->first == BI.character) // Does this person have a bounty on their head?
			{
				if (!AppendBountyInCfg(BI, false, false, 0, pRenameInfo->second, L"")) // If they do we need to update the bounty list with their new name
					continue;

				bountyList.remove(BI); // Remove the old bounty from the current active bounties
				BI.character = pRenameInfo->second; // Make the change
				bountyList.emplace_back(BI); // Replace the old one with the up to date one
			}

			if(pRenameInfo->first == BI.issuer)
			{
				if (!AppendBountyInCfg(BI, false, false, 0, L"", pRenameInfo->second))
					continue;

				bountyList.remove(BI); // Remove the old bounty from the current active bounties
				BI.character = pRenameInfo->second; // Make the change
				bountyList.emplace_back(BI); // Replace the old one with the up to date one
			}
		} // Continue to loop over incase they have multiple bounties on their head
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HTML / JSON Data Exports
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Exports the current bounty list to a HTML file
void ExportHTML()
{
	ofstream HTML; // New Stream

	HTML.open(scHTMLPath.c_str(), ios::out | ios::trunc); // Open up a HTML file for editing
	if (!HTML.is_open()) // If we were unable to do so, we are either missing permissions or the directory doesn't exist
	{
		ConPrint(L"ERROR: Failed to create HTML file\n"); // So we tell them that
		ConPrint(L"Filepath: %s\n", stows(scHTMLPath).c_str());
		ConPrint(L"Please make sure the directory exists and we have write permissions.\n");
		return;
	}

	HTML << "<html>" << endl; // Each use of 'endl' is the same as '\n'
	HTML << "<head><title>Player Bounty List</title><style type = text/css>" << endl;
	HTML << ".ColumnH {FONT-FAMILY: Tahoma; FONT-SIZE: 10pt;  TEXT-ALIGN: left; COLOR: #000000; BACKGROUND: #ECE9D8;}" << endl;
	HTML << ".Column0 {FONT-FAMILY: Tahoma; FONT-SIZE: 10pt;  TEXT-ALIGN: left; COLOR: #000000; BACKGROUND: #FFFFFF;}" << endl;
	HTML << "</style></head><body>\n" << endl;

	HTML << R"(<table width="90%%" border="1" cellspacing="0" cellpadding="2">)" << endl;

	HTML << "<tr>" << endl;
	HTML << "<th class=\"ColumnH\">Bounty Target</th>" << endl;
	HTML << "<th class=\"ColumnH\">Contact Payout</th>" << endl;
	HTML << "<th class=\"ColumnH\">Amount Claimable</th>" << endl;
	HTML << "<th class=\"ColumnH\">Expires In</th>" << endl;
	HTML << "<th class=\"ColumnH\">Issued By</th>" << endl;
	HTML << "<th class=\"ColumnH\">Reason</th>" << endl;
	HTML << "</tr>" << endl << endl;

	for (auto& iter : bountyList) // Iterate over our list of bounties
	{
		HTML << "<tr>" << endl; // Write up the information in a table
		HTML << "<td class=\"column0\">" << wstos(HtmlEncode(iter.character)).c_str() << "</td>" << endl;
		HTML << "<td class=\"column0\">" << wstos(ToMoneyStr(iter.bountyAmount)).c_str() << "</td>" << endl;
		HTML << "<td class=\"column0\">" << iter.claimedAmount - iter.claimAmount << "</td>" << endl;
		HTML << "<td class=\"column0\">" << (itos(iter.timeUntilDecay - 1) + " Day(s)").c_str() << "</td>" << endl;
		HTML << "<td class=\"column0\">" << wstos(HtmlEncode(iter.issuer)).c_str() << "</td>" << endl;
		HTML << "<td class=\"column0\">" << wstos(HtmlEncode(stows(iter.reason))).c_str() << "</td>" << endl;
	}

	HTML << "</table>" << endl << endl << "</body><html>" << endl; // Finish up
	HTML.close();
}

// We create a custom writer configuration for writing to our JSON file - cuts out many lines of code
namespace minijson {
	template<>
	struct default_value_writer<BOUNTYINFO>
	{
		void operator()(ostream& stream, const BOUNTYINFO& bi,
			writer_configuration configuration) const
		{
			object_writer writer(stream, configuration);
			writer.write("target", wstos(HtmlEncode(bi.character)).c_str());
			writer.write("amount", wstos(ToMoneyStr(bi.bountyAmount)).c_str());
			writer.write("claim", bi.claimAmount - bi.claimedAmount);
			writer.write("days", (itos(bi.timeUntilDecay - 1) + " Day(s)").c_str());
			writer.write("issuer", wstos(HtmlEncode(bi.issuer)).c_str());
			writer.write("reason", wstos(HtmlEncode(stows(bi.reason))).c_str());
			writer.close();
		}
	};
}

// We export our config to JSON as well
void ExportJSON()
{
	stringstream stream; // New Stringstream
	minijson::object_writer writer(stream); // Open writer
	writer.write("timestamp", to_iso_string(pt::second_clock::local_time())); // Log current time

	minijson::object_writer bounties = writer.nested_object("bounties"); // Open new object
	minijson::array_writer targets = bounties.nested_array("targets"); // Open new array

	for (auto& iter : bountyList) // Iterate over the list
		targets.write(iter); // write contents to each individual object

	targets.close(); // Close the array
	bounties.close(); // Close the named object
	writer.close(); // Close the entire JSON object

	ofstream JSON; // New Output Stream

	JSON.open(scJSONPath.c_str(), ios::out | ios::trunc); // Open new file for editing, delting the old contents if it exists
	if (!JSON.is_open()) // If we couldn't open it, there is either a missing directory or permission error
	{
		ConPrint(L"ERROR: Failed to create HTML file\n"); // We tell them this
		ConPrint(L"Filepath: %s\n", stows(scJSONPath).c_str());
		ConPrint(L"Please make sure the directory exists.\n");
		return;
	}

	JSON << stream.rdbuf(); // Write the stringstream to the file
	JSON.close(); // Close and save the file
}

// We want to write file every half hour
void HkTimerCheckKick()
{
	returncode = DEFAULT_RETURNCODE;

	const uint iCurrentTime = static_cast<uint>(time(nullptr)); // Get the current time

	if (iExportMode == 0 || iExportMode == 2) // Are we exporting to HTML or both HTML and JSON?
		if (iCurrentTime % 1800 == 0) // Is it the half hour mark?
			ExportHTML(); // If so, export to HTML

	if (iExportMode == 1 || iExportMode == 2) // Are we exporting to JSON or both HTML and JSON?
		if (iCurrentTime % 1800 == 0) // Is it the half hour mark?
			ExportJSON(); // If so, export to JSON
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
	{ L"/bounty new", UserCmd_BountyNew, L"Usage: /bounty new <target> <claimable x times> <payout per kill> <reason>" }, // Create a new Bounty
	{ L"/bounty help", UserCmd_BountyHelp, L"" }, // List Bounty Commands
	{ L"/bounty add", UserCmd_BountyAdd, L"Usage: /bounty add <target> <cash to add>" }, // Add cash to bounty target
	{ L"/bounty remove", UserCmd_BountyRemove, L"Usage: /bounty remove <target>" }, // Remove a bounty target
	{ L"/bounty list", UserCmd_BountyList, L"Usage: /bounty list [page]" }, // List all available bounties
	{ L"/bounty me", UserCmd_DoIHaveABounty, L"" }, // Check if the user has a bounty on them
	{ L"/bounty", UserCmd_BountyHelp, L"" }, // In case people forget the second word
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
	p_PI->sName = "Automated Bounty Hunting Plugin by Laz";
	p_PI->sShortName = "bountyplugin";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;

	p_PI->lstHooks.emplace_back(PLUGIN_HOOKINFO(reinterpret_cast<FARPROC*>(&LoadSettings), PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.emplace_back(PLUGIN_HOOKINFO(reinterpret_cast<FARPROC*>(&UserCmd_Process), PLUGIN_UserCmd_Process, 0));
	p_PI->lstHooks.emplace_back(PLUGIN_HOOKINFO(reinterpret_cast<FARPROC*>(&ShipDestroyed), PLUGIN_ShipDestroyed, 0));
	p_PI->lstHooks.emplace_back(PLUGIN_HOOKINFO(reinterpret_cast<FARPROC*>(&PlayerLaunch_AFTER), PLUGIN_HkIServerImpl_PlayerLaunch_AFTER, 0));
	p_PI->lstHooks.emplace_back(PLUGIN_HOOKINFO(reinterpret_cast<FARPROC*>(&HkTimerCheckKick), PLUGIN_HkTimerCheckKick, 0));
	p_PI->lstHooks.emplace_back(PLUGIN_HOOKINFO(reinterpret_cast<FARPROC*>(&Plugin_Communication_CallBack), PLUGIN_Plugin_Communication, 0));

	return p_PI;
}