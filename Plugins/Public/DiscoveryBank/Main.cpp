// Discovery Bank - A collection of cash based plugins, commands, and functions.
// By Laz with snippets from other plugins
// Created July 21st 2018
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

// We automatically have all the features of this disabled
bool bPluginEnabled = false; // *everything* is disabled
bool bPluginHookBank = false; // The FLHook money store is disabled
int iMoneyWarning = 1500000000; // Money at which we start warning people
uint iEraseOldBanks = 365; // How many days before we erase old FLHook banks

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Loading Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

vector<MONEY_STRUCT> vecMoneyStructs; // A vector of all our Banks
map<wstring, TRANSACTIONS> mapTransactionHistory; // A vector of all our transactions, bound to it's accociated bank account.

void UpdateBanks() // Resave all the banks
{
	char szDataPath[MAX_PATH];
	GetUserDataPath(szDataPath); // Get the My Documents\My Games\Freelancer\ Directory
	string scPath = string(szDataPath) + R"(\Accts\MultiPlayer\DiscoveryBank\__bank.ini)"; // Search inside the MP folder and create a new file inside a new folder

	FILE *file = fopen(scPath.c_str(), "wb"); // Write Binary to the file
	if (file) // If the file can be opened, open it
	{
		for (auto& vecMoneyStruct : vecMoneyStructs) // Loop over all our banks
		{
			fprintf(file, "[bank]\n"); // Header for each bank
			ini_write_wstring(file, "creator", vecMoneyStruct.wscCreator); // Who created it
			ini_write_wstring(file, "account", vecMoneyStruct.wscAccount); // Account name
			ini_write_wstring(file, "pass", vecMoneyStruct.wscPassword); // Password
			ini_write_wstring(file, "masterpass", vecMoneyStruct.wscMasterPassword); // Master Password
			fprintf(file, "cash = %lld\n", vecMoneyStruct.iCash); // Current cash stored
			fprintf(file, "accessed = %u\n", vecMoneyStruct.iLastAccessed); // When was the bank last used
		}
		fclose(file); // Close file
	}
}

void LogTransaction(const string& scFileName, const wstring& wscAccount)
{ // Save a new transaction to the log
	char szDataPath[MAX_PATH];
	GetUserDataPath(szDataPath); // Get the folder where our save data is located
	string scPath = string(szDataPath) + R"(\Accts\MultiPlayer\DiscoveryBank\)" + scFileName; // Each bank get's it's own file so things are easy to manage

	FILE *file = fopen(scPath.c_str(), "wb"); // Write binary to the file
	if (file)
	{
		auto i = mapTransactionHistory.find(wscAccount); // Get the map where our transactions are stored
		if (i != mapTransactionHistory.end()) // If we found the map we are after
		{
			fprintf(file, "[Transaction Log]\n"); // Ini Header
			fprintf(file, "account = %ls\n", wscAccount.c_str()); // Account name
			fprintf(file, "created = %ls\n", i->second.wscCreated.c_str()); // When was it created?
			for (const auto& wscTransaction : i->second.vecTransactions) // Loop over all the different transactions
				fprintf(file, "transaction = %ls\n", wscTransaction.c_str()); // Write each one to the file
			fclose(file); // Close file
		}
	}
}

void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof (szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + R"(\flhook_plugins\DiscoveryBank.cfg)"; // The plugin settings

	bPluginEnabled = IniGetB(scPluginCfgFile, "Features", "any", false); // Disable all plugin features
	bPluginHookBank = IniGetB(scPluginCfgFile, "Features", "bank", false); // Disable features specific to the Discovery Banking System
	iMoneyWarning = IniGetI(scPluginCfgFile, "Config", "moneyWarning", iMoneyWarning); // At what ship value do we start alerting people they are nearing the cap?
	iEraseOldBanks = IniGetI(scPluginCfgFile, "Config", "moneyErase", iEraseOldBanks); // After how many days do we delete old banks?

	string scUserBankPath = string(szCurDir) + R"(\Accts\MultiPlayer\DiscoveryBank\__bank.ini)"; // Load in the bank file
	string scUserBankTransactionPath = string(szCurDir) + R"(\Accts\MultiPlayer\DiscoveryBank\)"; // Where all our transaction logs are located
	CreateDirectoryA(scUserBankTransactionPath.c_str(), nullptr); // If the directory doesn't exist, we will need to make it
	scUserBankTransactionPath = scUserBankTransactionPath + "bank_*.ini"; // Prefix with wildcard. All transaction logs will follow this format.

	WIN32_FIND_DATA findfile; // We need a file handler to loop over all files in the directory
	HANDLE h = FindFirstFile(scUserBankTransactionPath.c_str(), &findfile); // Load in the right file path
	if (h != INVALID_HANDLE_VALUE)
	{
		do
		{
			string filepath = string(szCurDir) + R"(\Accts\MultiPlayer\player_bases\DiscoveryBank\)" + findfile.cFileName; // The first of the wildcard files we loaded
			if (filepath == string(szCurDir) + R"(\Accts\MultiPlayer\player_bases\DiscoveryBank\)" + "__bank.ini") // If it's the bank file, ignore
				continue;

			INI_Reader ini; // Create a new ini reader instance to iterate over the transaction log
			if (ini.open(filepath.c_str(), false)) // Open
				while (ini.read_header())
					if (ini.is_header("Transaction Log")) // If the header is [Transaction Log]
					{
						TRANSACTIONS t; // Create a new struct
						wstring wscAccount; // The name of the account for the map
						vector<wstring> vecTransactions; // Empty vector for the logs
						while (ini.read_value())
						{
							if (ini.is_value("account"))
								wscAccount = stows(ini.get_value_string()); // Set the account name so we can put it as the key for the map
							else if (ini.is_value("created")) // Add the time it was created
								t.wscCreated = stows(ini.get_value_string());
							else if (ini.is_value("transaction")) // Now add all those transactions
								t.vecTransactions.emplace_back(stows(ini.get_value_string()));
						}
						mapTransactionHistory[wscAccount] = t; // Add this file's data to the map
					}
			ini.close();
		} while (FindNextFile(h, &findfile)); // Repeat until we run out of files
		FindClose(h); // We run out of files
	}

	INI_Reader ini; // This time we are reading the bank file
	if (ini.open(scUserBankPath.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("bank"))
			{
				MONEY_STRUCT CASH; // New Bank struct
				while (ini.read_value())
				{
					wstring wscCreator, wscAccount, wscPass; // The different fields we have to define in advance
					if (ini.is_value("creator"))
					{
						ini_get_wstring(ini, wscCreator); // Set wscCreator to equal the current value being read
						CASH.wscCreator = wscCreator; // Assign that value in the struct
					}
					else if (ini.is_value("account"))
					{
						ini_get_wstring(ini, wscAccount); // Rinse 
						CASH.wscAccount = wscAccount; // Repeat
					}
					else if (ini.is_value("pass"))
					{
						ini_get_wstring(ini, wscPass);
						CASH.wscAccount = wscPass;
					}
					else if (ini.is_value("cash"))
					{
						CASH.iCash = stoll(ini.get_value_string()); // Convert the string to a long long (INT64)
					}
					else if (ini.is_value("accessed"))
					{
						CASH.iLastAccessed = ini.get_value_int(0); // When was the bank last accssed (in ms)
					}
				}
				if (CASH.iLastAccessed > 1000 * 60 * 60 * 24 * iEraseOldBanks + static_cast<uint>(time(nullptr)))
					continue; // Don't include any banks that are more than the amount of days specified (1 year by default)
				vecMoneyStructs.emplace_back(CASH); // Add the bank to the list
			}
		}
	}
	ini.close();
	UpdateBanks(); // Any banks not in the list will be automatically deleted when the UpdateBanks() function is run
	ConPrint(L"DiscoveryBank: Loaded %u Banks.\n", vecMoneyStructs.size());
	LoadSettings();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UserCmd_GetShipValue(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{ // A small function that allows someone to check how much their ship is worth + credits are worth
	int iPlayerCash;
	float fShipValue;
	pub::Player::InspectCash(iClientID, iPlayerCash); // Get how much cash they have
	pub::Player::GetAssetValue(iClientID, fShipValue); // Get their ship value
	const int iTotalValue = static_cast<int>(iPlayerCash + fShipValue); // Add them together and round up so we don't have any weird data loss.
	PrintUserCmdText(iClientID, L"Your ship is currently valued at: %s credits", ToMoneyStr(iTotalValue).c_str()); // Tell them how much their ship is valued at
	if (iTotalValue > iMoneyWarning) // If they are above the amount we warn them at, we better warn them.
	{
		PrintUserCmdText(iClientID, L"WARNING: Your ship value is currently over %s credits.", ToMoneyStr(iMoneyWarning).c_str());
		PrintUserCmdText(iClientID, L"If your ship reaches over 2.1 billion credits, your character will become corrupted. Prevent this by using /bank");
	}
	return true;
}

bool BankExists(const wstring& wscAccount) // A function that allows us to run a direct look up on a struct
{
	const auto predicate = [wscAccount](const MONEY_STRUCT & c) // What variable will we be using to compare in the struct
	{
		return c.wscAccount == wscAccount; // What field are we comparing against?
	};
	const bool bExists = find_if(begin(vecMoneyStructs), end(vecMoneyStructs), predicate) != end(vecMoneyStructs); // Return true if found, false if not.
	return bExists; // Return the result
}

bool CheckEmpty(uint iClientID, const wstring& wscAccount, const wstring& wscPassword) // We check these with literally every function. Prevent code duplication
{
	if (wscAccount.empty()) // If no account name provided
	{
		PrintUserCmdText(iClientID, L"ERR: No account name provided."); // Yell at the person
		return false; // Tell the main function not to continue
	}

	if (wscPassword.empty()) // If no password provided
	{
		PrintUserCmdText(iClientID, L"ERR: No password provided."); // Yell at the person
		return false; // Tell the main function not to continue
	}

	return true; // Everything is fine, lets continue
}

bool UserCmd_BankInfo(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginHookBank)
	{
		PrintUserCmdText(iClientID, L"The Bank Plugin is disabled."); // Don't display this if the bank is disabled
		return true;
	}

	const wstring wscBankHelp = L"<RDL><PUSH/>" // An XML string for our help menu! Help menus are great.
		L"<TRA bold=\"true\"/><TEXT>/bank</TEXT><TRA bold=\"false\"/><PARA/>"
		L"<TEXT>Shows this help menu.</TEXT><PARA/><PARA/>"

		L"<TRA bold=\"true\"/><TEXT>/bank create [account] [password]</TEXT><TRA bold=\"false\"/><PARA/>"
		L"<TEXT>This creates a new external bank. The account name and password will be the credentials you use to access it.</TEXT><PARA/><PARA/>"

		L"<TRA bold=\"true\"/><TEXT>/bank withdraw [account] [password] [amount]</TEXT><TRA bold=\"false\"/><PARA/>"
		L"<TEXT>Withdraw money from your bank. Login with the account and password, then specify how much you want to withdraw.</TEXT><PARA/><PARA/>"

		L"<TRA bold=\"true\"/><TEXT>/bank deposit [account] [password] [amount]</TEXT><TRA bold=\"false\"/><PARA/>"
		L"<TEXT>Deposit money inside your bank for later use.</TEXT><PARA/><PARA/>"

		L"<TRA bold=\"true\"/><TEXT>/bank show [account] [password]</TEXT><TRA bold=\"false\"/><PARA/>"
		L"<TEXT>Find out how much money is in the bank currently.</TEXT><PARA/><PARA/>"

		L"<TRA bold=\"true\"/><TEXT>/bank transfer [account] [password] [amount] [target bank]</TEXT><TRA bold=\"false\"/><PARA/>"
		L"<TEXT>Transfer a specified amount of funds from your bank directly to someone else's bank. Target account is their account name.</TEXT><PARA/><PARA/>"

		L"<TRA bold=\"true\"/><TEXT>/bank history [account] [password] [amount] | OR | /bank history [account] [password] all</TEXT><TRA bold=\"false\"/><PARA/>"
		L"<TEXT>Find out your transaction history. Using all instead of a number, will get all of your transaction records. A number will get the amount specified.</TEXT><PARA/><PARA/>"

		L"<TRA bold=\"true\"/><TEXT>/bank password [account] [master password] [new pass]</TEXT><TRA bold=\"false\"/><PARA/>"
		L"<TEXT>Create a secondary password for your bank. Useful if you are a faction leader and have a shared bank.</TEXT><PARA/><PARA/>"

		L"<TRA bold=\"true\"/><TEXT>/bank masterpass [account] [master password] [new master pass]</TEXT><TRA bold=\"false\"/><PARA/>"
		L"<TEXT>Update the master password for your bank. Make sure you write it down!</TEXT><PARA/><PARA/>"
		L"<POP/></RDL>";

	HkChangeIDSString(iClientID, 500000, L"Discovery Bank Help Menu"); // The title for our helpmenu.
	HkChangeIDSString(iClientID, 500001, wscBankHelp); // That XML string we just wrote

	FmtStr caption(0, 0);
	caption.begin_mad_lib(500000);
	caption.end_mad_lib();

	FmtStr message(0, 0);
	message.begin_mad_lib(500001);
	message.end_mad_lib();

	pub::Player::PopUpDialog(iClientID, caption, message, POPUPDIALOG_BUTTONS_CENTER_OK); // Popup that helpmenu
	return true;
}

bool UserCmd_CreateBank(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginHookBank)
		return true;

	const wstring wscCharacter = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID)); // Active character name
	const wstring wscAccount = ToLower(GetParam(wscParam, ' ', 0)); // Account name. All account names must be lowercase so we don't have duplicates filenames.
	const wstring wscPass = GetParam(wscParam, ' ', 1); // The password

	if (!CheckEmpty(iClientID, wscAccount, wscPass)) // Run our check function
	{
		PrintUserCmdText(iClientID, usage); // If it failed print the usage and return
		return true;
	}

	const bool bExists = BankExists(wscAccount); // If the bank exists we can continue

	if (bExists)
	{ // Doesn't exist lets stop right here and yell at them for it.
		PrintUserCmdText(iClientID, L"ERR: A bank account with this name already exists.");
		PrintUserCmdText(iClientID, L"Please select a different account name.");
		return true;
	}

	MONEY_STRUCT c; // A new money struct
	c.wscCreator = wscCharacter; // Same old
	c.wscAccount = wscAccount; // Same old
	c.wscPassword = wscPass; // We set the master pass and normal pass to be the same by default
	c.wscMasterPassword = wscPass;
	c.iCash = 0;
	c.iLastAccessed = static_cast<uint>(time(nullptr)); // Set the current time (in ms)

	vecMoneyStructs.emplace_back(c); // Place that struct in the list
	UpdateBanks(); // Save this bank to the config
	PrintUserCmdText(iClientID, L"New Bank Created. Account: %s - Password: %s", c.wscAccount.c_str(), c.wscMasterPassword.c_str());
	PrintUserCmdText(iClientID, L"DO NOT FORGET THIS."); // Alert them about what they've done.

	TRANSACTIONS t; // Create a new transaction log
	const vector<wstring> vecTransactions; // An empty list to use for now
	t.vecTransactions = vecTransactions; // set that empty list
	t.wscCreated = GetTimeString(false); // Get current server time
	mapTransactionHistory[wscAccount] = t; // Add them to the map
	LogTransaction("bank_" + wstos(wscAccount) + ".ini", wscAccount); // Save it!
	return true;
}

bool UserCmd_WithdrawBankCash(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginHookBank)
		return true;

	const wstring wscAccount = ToLower(GetParam(wscParam, ' ', 0));
	const wstring wscPassword = GetParam(wscParam, ' ', 1);
	const int iCash = ToInt(GetParam(wscParam, ' ', 2));

	if (!CheckEmpty(iClientID, wscAccount, wscPassword))
	{
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	if (iCash <= 0) // If the amount provided is not a number, 0, less than zero, or greater than an int can store.
	{
		PrintUserCmdText(iClientID, L"ERR: Invalid Perameters.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	const bool bExists = BankExists(wscAccount);

	if (!bExists)
	{
		PrintUserCmdText(iClientID, L"ERR: There is no bank account with that name.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	auto i = find_if(vecMoneyStructs.begin(), vecMoneyStructs.end(), FIND_MONEY_STRUCT(wscAccount)); // Direct look up of the struct
	if (i->wscAccount != wscAccount) // If some reason it fails we have a backup and things don't crash.
	{
		PrintUserCmdText(iClientID, L"ERR: Unable to find account to withdraw from.");
		return true;
	}

	if (i->wscPassword != wscPassword && i->wscMasterPassword != wscPassword) // If the password provided fails to match both of the stored ones
	{
		PrintUserCmdText(iClientID, L"ERR: Invalid Passcode."); // Tell them about it
		PrintUserCmdText(iClientID, usage); // Tell them again
		return true;
	}

	if (i->iCash < iCash) // If the bank doesn't have enough money
	{
		PrintUserCmdText(iClientID, L"ERR: Not enough cash in bank. Bank only has %s", Int64ToPrettyStr(i->iCash).c_str());
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	float fAssetValue;
	pub::Player::GetAssetValue(iClientID, fAssetValue);
	int iCurrMoney;
	pub::Player::InspectCash(iClientID, iCurrMoney);

	const int iTotalValue = static_cast<int>(iCurrMoney + fAssetValue);

	if (iTotalValue + iCash > 2000000000) // If it pushes them too close to the cap we should stop them
	{
		PrintUserCmdText(iClientID, L"ERR: Withdraw will exceed credit limit.");
		return true;
	}

	pub::Player::AdjustCash(iClientID, iCash); // Add money to the player
	HkSaveChar(iClientID);
	i->iCash -= iCash; // Take money from the bank
	i->iLastAccessed = static_cast<uint>(time(nullptr)); // Update the bank's last use
	UpdateBanks(); // Save the config
	PrintUserCmdText(iClientID, L"You have successfully withdrawn %s credits.", Int64ToPrettyStr(iCash).c_str()); // Tell them about their success

																												  // Now it's time to log stuff
	wstring wscLogMessage = L"%time%- %charname% has withdrawn %credits% credits from the %bank% bank account.";
	wscLogMessage = ReplaceStr(wscLogMessage, L"%time%", GetTimeString(false));
	wscLogMessage = ReplaceStr(wscLogMessage, L"%charname%", reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID)));
	wscLogMessage = ReplaceStr(wscLogMessage, L"%credits%", Int64ToPrettyStr(iCash));
	wscLogMessage = ReplaceStr(wscLogMessage, L"%bank%", wscAccount);


	auto map = mapTransactionHistory.find(wscAccount);
	if (map != mapTransactionHistory.end())
	{
		map->second.vecTransactions.emplace_back(wscLogMessage);
		LogTransaction("bank_" + wstos(wscAccount) + ".ini", wscAccount);
	}
	return true;
}

bool UserCmd_DepositBankCash(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginHookBank)
		return true;

	const wstring wscCharacter = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID));
	const wstring wscAccount = ToLower(GetParam(wscParam, ' ', 0));
	const wstring wscPassword = GetParam(wscParam, ' ', 1);
	const int iCash = ToInt(GetParam(wscParam, ' ', 2));

	if (!CheckEmpty(iClientID, wscAccount, wscPassword))
	{
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	if (iCash <= 0)
	{
		PrintUserCmdText(iClientID, L"ERR: Invalid Perameters.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	const bool bExists = BankExists(wscAccount);

	if (!bExists)
	{
		PrintUserCmdText(iClientID, L"ERR: There is no bank account with that name.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	auto i = find_if(vecMoneyStructs.begin(), vecMoneyStructs.end(), FIND_MONEY_STRUCT(wscAccount));
	if (i->wscAccount != wscAccount)
	{
		PrintUserCmdText(iClientID, L"ERR: Unable to find account to deposit into.");
		return true;
	}

	if (i->wscPassword != wscPassword && i->wscMasterPassword != wscPassword)
	{
		PrintUserCmdText(iClientID, L"ERR: Invalid Passcode.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}
	int iCurrMoney;
	pub::Player::InspectCash(iClientID, iCurrMoney);

	if (iCurrMoney < iCash)
	{
		PrintUserCmdText(iClientID, L"ERR: You do not have enough funds to transfer the requested amount.");
		return true;
	}

	HK_ERROR err;
	if ((err = HkAddCash(wscCharacter, 0 - iCash)) != HKE_OK)
	{
		PrintUserCmdText(iClientID, L"ERR: Unable to transfer credits.");
		PrintUserCmdText(iClientID, L"ERR: " + HkErrGetText(err));
		return true;
	}

	HkSaveChar(iClientID);
	i->iCash += iCash;
	i->iLastAccessed = static_cast<uint>(time(nullptr));
	UpdateBanks();
	PrintUserCmdText(iClientID, L"You have successfully deposited %s credits.", Int64ToPrettyStr(iCash).c_str());

	wstring wscLogMessage = L"%time%- %charname% has deposited %credits% credits into the %bank% bank account.";
	wscLogMessage = ReplaceStr(wscLogMessage, L"%time%", GetTimeString(false));
	wscLogMessage = ReplaceStr(wscLogMessage, L"%charname%", reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID)));
	wscLogMessage = ReplaceStr(wscLogMessage, L"%credits%", Int64ToPrettyStr(iCash));
	wscLogMessage = ReplaceStr(wscLogMessage, L"%bank%", wscAccount);

	auto map = mapTransactionHistory.find(wscAccount);
	if (map != mapTransactionHistory.end())
	{
		map->second.vecTransactions.emplace_back(wscLogMessage);
		LogTransaction("bank_" + wstos(wscAccount) + ".ini", wscAccount);
	}
	return true;
}

bool UserCmd_TransferBankCash(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{ // This function allows us to transfer cash from bank account to another, without actually needing another character. A direct transfer as it were.
	if (!bPluginHookBank)
		return true;

	const wstring wscAccount = ToLower(GetParam(wscParam, ' ', 0)); // Your bank account
	const wstring wscPassword = GetParam(wscParam, ' ', 1); // Your bank account passcode
	const int iCash = ToInt(GetParam(wscParam, ' ', 2)); // How much are you transfering
	const wstring wscTarget = GetParam(wscParam, ' ', 3); // Target Bank Account

	if (!CheckEmpty(iClientID, wscAccount, wscPassword))
	{
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	if (iCash <= 0)
	{
		PrintUserCmdText(iClientID, L"ERR: Invalid Perameters.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	if (wscTarget.empty())
	{
		PrintUserCmdText(iClientID, L"ERR: Invalid Perameters.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	const bool bBankExists = BankExists(wscAccount);
	const bool bTargetBankExists = BankExists(wscTarget);

	if (!bBankExists)
	{
		PrintUserCmdText(iClientID, L"ERR: Bank to deduct from does not exist. Please check name.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	if (!bTargetBankExists)
	{
		PrintUserCmdText(iClientID, L"ERR: Bank to transfer to does not exist. Please check name.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	auto i = find_if(vecMoneyStructs.begin(), vecMoneyStructs.end(), FIND_MONEY_STRUCT(wscAccount));
	if (i->wscAccount != wscAccount)
	{
		PrintUserCmdText(iClientID, L"ERR: Unable to find account to withdraw from.");
		return true;
	}

	if (i->wscPassword != wscPassword && i->wscMasterPassword != wscPassword)
	{
		PrintUserCmdText(iClientID, L"ERR: Invalid Passcode.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	if (i->iCash < iCash)
	{
		PrintUserCmdText(iClientID, L"ERR: Not enough cash in bank. Bank only has %s", Int64ToPrettyStr(i->iCash).c_str());
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	auto ms = find_if(vecMoneyStructs.begin(), vecMoneyStructs.end(), FIND_MONEY_STRUCT(wscTarget));
	if (ms->wscAccount != wscTarget)
	{
		PrintUserCmdText(iClientID, L"ERR: Cannot find target account.");
		return true;
	}

	i->iCash -= iCash; // Minus the cash from our bank
	i->iLastAccessed = static_cast<uint>(time(nullptr)); // Update our bank's last usage
	ms->iCash += iCash; // Add the money to his bank
	ms->iLastAccessed = static_cast<uint>(time(nullptr)); // Update his bank's last usage

	UpdateBanks();
	PrintUserCmdText(iClientID, L"%s - You have successfully transfered %s credits to %s.",
		GetTimeString(false).c_str(), Int64ToPrettyStr(iCash).c_str(), wscTarget.c_str());

	auto map = mapTransactionHistory.find(wscAccount);
	wstring wscLogMessage;
	if (map != mapTransactionHistory.end())
	{
		wscLogMessage = L"%time%- %charname% has transfered %credits% credits from %bank% to %target%.";
		wscLogMessage = ReplaceStr(wscLogMessage, L"%time%", GetTimeString(false));
		wscLogMessage = ReplaceStr(wscLogMessage, L"%charname%", reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID)));
		wscLogMessage = ReplaceStr(wscLogMessage, L"%credits%", Int64ToPrettyStr(iCash));
		wscLogMessage = ReplaceStr(wscLogMessage, L"%bank%", wscAccount);
		wscLogMessage = ReplaceStr(wscLogMessage, L"%target%", wscTarget);

		map->second.vecTransactions.emplace_back(wscLogMessage);
		LogTransaction("bank_" + wstos(wscAccount) + ".ini", wscAccount);
	}

	map = mapTransactionHistory.find(wscTarget); // We now need to find the bank of the guy who recieved cash, and log it there as well.
	if (map != mapTransactionHistory.end())
	{
		map->second.vecTransactions.emplace_back(wscLogMessage); // We'll reuse the same message as it still tells us everything we need.
		LogTransaction("bank_" + wstos(wscAccount) + ".ini", wscAccount);
	}

	return true;
}

bool UserCmd_ShowBankCash(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginHookBank)
		return true;

	const wstring wscAccount = ToLower(GetParam(wscParam, ' ', 0));
	const wstring wscPassword = GetParam(wscParam, ' ', 1);

	if (!CheckEmpty(iClientID, wscAccount, wscPassword))
	{
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	const bool bExists = BankExists(wscAccount);

	if (!bExists)
	{
		PrintUserCmdText(iClientID, L"ERR: There is no bank account with that name.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	auto i = find_if(vecMoneyStructs.begin(), vecMoneyStructs.end(), FIND_MONEY_STRUCT(wscAccount));
	if (i->wscAccount != wscAccount)
	{
		PrintUserCmdText(iClientID, L"ERR: Unable to find account.");
		return true;
	}

	if (i->wscPassword != wscPassword && i->wscMasterPassword != wscPassword)
	{
		PrintUserCmdText(iClientID, L"ERR: Invalid Passcode.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	PrintUserCmdText(iClientID, L"You have %s credits in the bank.", Int64ToPrettyStr(i->iCash).c_str());
	return true;
}

bool UserCmd_BankHistory(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginHookBank)
		return true;

	const wstring wscAccount = ToLower(GetParam(wscParam, ' ', 0));
	const wstring wscPassword = GetParam(wscParam, ' ', 1);
	const wstring wscAmount = GetParam(wscParam, ' ', 2);

	if (!CheckEmpty(iClientID, wscAccount, wscPassword))
	{
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	if (wscAmount.empty())
	{
		PrintUserCmdText(iClientID, L"ERR: No amount specified.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	const bool bExists = BankExists(wscAccount);

	if (!bExists)
	{
		PrintUserCmdText(iClientID, L"ERR: There is no bank account with that name.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	auto i = find_if(vecMoneyStructs.begin(), vecMoneyStructs.end(), FIND_MONEY_STRUCT(wscAccount));
	if (i->wscAccount != wscAccount)
	{
		PrintUserCmdText(iClientID, L"ERR: Bank does not exist.");
		return true;
	}

	if (i->wscPassword != wscPassword && i->wscMasterPassword != wscPassword)
	{
		PrintUserCmdText(iClientID, L"ERR: Invalid Passcode.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	auto map = mapTransactionHistory.find(wscAccount);
	if (map != mapTransactionHistory.end())
	{
		vector<wstring> vecTransactions = map->second.vecTransactions;
		const int iAmount = ToInt(wscAmount);

		if (wscAmount == L"all") // If they specify all, rather than a number, we list them all their transactions
		{
			for (const auto& wscTransaction : vecTransactions) // So we loop over each one
				PrintUserCmdText(iClientID, L"%s", wscTransaction.c_str()); // and print it
		}

		else if (iAmount > 0 && iAmount <= static_cast<int>(vecTransactions.size())) // If the amount they specified is more than 0 and less than the max
		{	// Loop over all the transactions. We simply minus the amount of transactions we want to see from the max, and then use that as our starting index
			for (int i = vecTransactions.size() - iAmount - 1; i < static_cast<int>(vecTransactions.size()); ++i)
				PrintUserCmdText(iClientID, L"%s", vecTransactions[i].c_str()); // Print each one
		}

		else
			PrintUserCmdText(iClientID, L"ERR: You did not specify an amount of transactions to view."); // It must be their fault. Obvs
	}
	return true;
}

bool UserCmd_UpdateBankPass(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginHookBank)
		return true;

	const wstring wscAccount = ToLower(GetParam(wscParam, ' ', 0));
	const wstring wscPass = GetParam(wscParam, ' ', 1);
	const wstring wscNewPass = GetParam(wscParam, ' ', 2);

	if (!CheckEmpty(iClientID, wscAccount, wscPass))
	{
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	if (wscNewPass.empty())
	{
		PrintUserCmdText(iClientID, L"ERR: Invalid Perameters.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}
	const bool bBankExists = BankExists(wscAccount);

	if (!bBankExists)
	{
		PrintUserCmdText(iClientID, L"ERR: Bank to deduct from does not exist. Please check name.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	auto i = find_if(vecMoneyStructs.begin(), vecMoneyStructs.end(), FIND_MONEY_STRUCT(wscAccount));
	if (i->wscAccount != wscAccount)
	{
		PrintUserCmdText(iClientID, L"ERR: Unable to find account to update.");
		return true;
	}

	if (i->wscMasterPassword != wscPass)
	{
		PrintUserCmdText(iClientID, L"ERR: Invalid Passcode.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	i->iLastAccessed = static_cast<uint>(time(nullptr));
	i->wscPassword = wscNewPass;

	UpdateBanks();
	PrintUserCmdText(iClientID, L"%s - Bank Pass Updated. Account: %s - Password: %s",
		GetTimeString(false).c_str(), i->wscAccount.c_str(), i->wscPassword.c_str());

	auto map = mapTransactionHistory.find(wscAccount);
	if (map != mapTransactionHistory.end())
	{
		wstring wscLogMessage = L"%time%- %charname% has updated the bank password of %bank%. Old Pass: %old% | New Pass: %new%";
		wscLogMessage = ReplaceStr(wscLogMessage, L"%time%", GetTimeString(false));
		wscLogMessage = ReplaceStr(wscLogMessage, L"%charname%", reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID)));
		wscLogMessage = ReplaceStr(wscLogMessage, L"%old%", wscPass);
		wscLogMessage = ReplaceStr(wscLogMessage, L"%bank%", wscAccount);
		wscLogMessage = ReplaceStr(wscLogMessage, L"%new%", wscNewPass);

		map->second.vecTransactions.emplace_back(wscLogMessage);
		LogTransaction("bank_" + wstos(wscAccount) + ".ini", wscAccount);
	}

	return true;
}

bool UserCmd_UpdateMasterBankPass(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	if (!bPluginHookBank)
		return true;

	const wstring wscAccount = ToLower(GetParam(wscParam, ' ', 0));
	const wstring wscPass = GetParam(wscParam, ' ', 1);
	const wstring wscNewPass = GetParam(wscParam, ' ', 2);

	if (!CheckEmpty(iClientID, wscAccount, wscPass))
	{
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	if (wscNewPass.empty())
	{
		PrintUserCmdText(iClientID, L"ERR: Invalid Perameters.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}
	const bool bBankExists = BankExists(wscAccount);

	if (!bBankExists)
	{
		PrintUserCmdText(iClientID, L"ERR: Bank to deduct from does not exist. Please check name.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	auto i = find_if(vecMoneyStructs.begin(), vecMoneyStructs.end(), FIND_MONEY_STRUCT(wscAccount));
	if (i->wscAccount != wscAccount)
	{
		PrintUserCmdText(iClientID, L"ERR: Unable to find account to update.");
		return true;
	}

	if (i->wscMasterPassword != wscPass)
	{
		PrintUserCmdText(iClientID, L"ERR: Invalid Passcode.");
		PrintUserCmdText(iClientID, usage);
		return true;
	}

	i->iLastAccessed = static_cast<uint>(time(nullptr));
	i->wscMasterPassword = wscNewPass;

	UpdateBanks();
	PrintUserCmdText(iClientID, L"%s- Master Bank Pass Updated. Account: %s - Password: %s",
		GetTimeString(false).c_str(), i->wscAccount.c_str(), i->wscMasterPassword.c_str());

	auto map = mapTransactionHistory.find(wscAccount);
	if (map != mapTransactionHistory.end())
	{
		wstring wscLogMessage = L"%time%- %charname% has updated the master bank password of %bank%. Old Pass: %old% | New Pass: %new%";
		wscLogMessage = ReplaceStr(wscLogMessage, L"%time%", GetTimeString(false));
		wscLogMessage = ReplaceStr(wscLogMessage, L"%charname%", reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID)));
		wscLogMessage = ReplaceStr(wscLogMessage, L"%old%", wscPass);
		wscLogMessage = ReplaceStr(wscLogMessage, L"%bank%", wscAccount);
		wscLogMessage = ReplaceStr(wscLogMessage, L"%new%", wscNewPass);

		map->second.vecTransactions.emplace_back(wscLogMessage);
		LogTransaction("bank_" + wstos(wscAccount) + ".ini", wscAccount);
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Events
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall GFGoodSell_AFTER(struct SGFGoodSellInfo const &gsi, unsigned int iClientID)
{
	returncode = DEFAULT_RETURNCODE;

	if(!bPluginHookBank)
		return;

	HK_ERROR err;

	const wstring wscCharname = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID));
	int iCash = 0;
	if ((err = HkGetCash(wscCharname, iCash)) != HKE_OK)
		return;

	if(iCash >= iMoneyWarning)
	{
		PrintUserCmdText(iClientID, L"Warning: You currently have over %s Credits. You are getting close to the credit limit.", ToMoneyStr(iMoneyWarning).c_str());
		PrintUserCmdText(iClientID, L"Please lower the amount of cash you have on this ship by sendng it to another ship, or by storing it in the bank. Type /bank for more information.");
	}
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
	{ L"/value", UserCmd_GetShipValue, L"" },
	
	// Bank Commands
	{ L"/bank create", UserCmd_CreateBank, L"/bank create <account> <password>" },
	{ L"/bank withdraw", UserCmd_WithdrawBankCash, L"/bank withdraw <account> <password> <amount>" },
	{ L"/bank deposit", UserCmd_DepositBankCash, L"/bank deposit <account> <password> <amount>" },
	{ L"/bank show", UserCmd_ShowBankCash, L"/bank show <account> <password>" },
	{ L"/bank transfer", UserCmd_TransferBankCash, L"/bank transfer <account> <password> <amount> <target>" },
	{ L"/bank history", UserCmd_BankHistory, L"/bank history <account> <password> <amount of transactions to view> OR /bank history <account> <password> all" },
	{ L"/bank masterpass", UserCmd_UpdateMasterBankPass, L"/bank masterpass <account> <master password> <new master pass>" },
	{ L"/bank pass", UserCmd_UpdateBankPass, L"/bank pass <account> <master password> <new pass>" },
	{ L"/bank", UserCmd_BankInfo, L"" }, // Put at the end because otherwise it will be the default option for anything with prefix of /bank
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
	p_PI->sName = "Discovery Bank by Laz";
	p_PI->sShortName = "discoverybank";
	p_PI->bMayPause = false;
	p_PI->bMayUnload = false;
	p_PI->ePluginReturnCode = &returncode;

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&GFGoodSell_AFTER, PLUGIN_HkIServerImpl_GFGoodSell_AFTER, 0));

	return p_PI;
}