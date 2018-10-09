#pragma once
#include <string>
#include <algorithm>
#include <list>
#include <FLHook.h>
#include <utility>
#include <PluginUtilities.h>

using namespace std;

struct MONEY_STRUCT // A struct where we store all the information on each bank
{
	wstring wscCreator; // Who created the bank
	wstring wscAccount; // Bank name
	wstring wscMasterPassword; // Master password of the bank, can set other passwords.
	wstring wscPassword; // Regular password, cannot change itself or master.
	INT64 iCash; // How much money is stored here
	uint iLastAccessed; // When was the bank last used?
};

struct FIND_MONEY_STRUCT // A comparing struct that allows us to preform direct lookups on lists/vectors of the MONEY_STRUCT type.
{
	wstring wscCompareAccount; // The value we want to preform a direct look up on, in this case account name.
	explicit FIND_MONEY_STRUCT(const wstring &wscAccount) : wscCompareAccount(wscAccount) { } // value of the struct that we are going to compare against
	bool operator () (MONEY_STRUCT const &c) const
	{
		return c.wscAccount == wscCompareAccount; // Return true for the find_if function, if the the account we are after is found
	}
};

struct TRANSACTIONS // A vector of all the transactions, with an extra field. 
{					// Doing it this way so I don't end up with a map<wstring, <map<wstring, vector<wstring>>>
	vector<wstring> vecTransactions; // All our different types of bank interactions (e.g. withdraw, deposit, password change)
	wstring wscCreated; // When was the bank created?
};