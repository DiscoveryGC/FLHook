#include "Hook.h"

list<stHelpEntry> lstHelpEntries;
bool get_bTrue(uint iClientID) { return true; }
void HkAddHelpEntry(const wstring &wscCommand, const wstring &wscArguments, const wstring &wscShortHelp, const wstring &wscLongHelp, _HelpEntryDisplayed fnIsDisplayed) {
	foreach(lstHelpEntries, stHelpEntry, he) {
		if (he->wszCommand == wscCommand && he->wszArguments == wscArguments)
			return;
	}
	stHelpEntry he;
	he.fnIsDisplayed = fnIsDisplayed;
	he.wszArguments = wscArguments;
	he.wszCommand = wscCommand;
	he.wszLongHelp = wscLongHelp;
	for (uint a = 0; a < he.wszLongHelp.length(); a++) {
		if (he.wszLongHelp[a] == '\t') {
			he.wszLongHelp = he.wszLongHelp.replace(a, 1, L"  ");
			a += 3;
		}
	}

	he.wszShortHelp = wscShortHelp;
	lstHelpEntries.push_back(he);
}
void HkRemoveHelpEntry(const wstring &wscCommand, const wstring &wscArguments) {
	foreach(lstHelpEntries, stHelpEntry, he) {
		if (he->wszCommand == wscCommand && he->wszArguments == wscArguments)
			lstHelpEntries.erase(he);

	}
}