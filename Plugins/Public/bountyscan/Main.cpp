// BountyScan Plugin by Alex. Just looks up the target's ID (tractor). For convenience on Discovery.
// 
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

#include <windows.h>
#include <list>
#include <FLHook.h>
#include <plugin.h>
#include <PluginUtilities.h>
#include "Main.h"

PLUGIN_RETURNCODE returncode;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		HkLoadStringDLLs();
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
		HkUnloadStringDLLs();
	}
	return true;
}

EXPORT PLUGIN_RETURNCODE Get_PluginReturnCode()
{
	return returncode;
}

bool bPluginEnabled = true;


bool UserCmd_BountyScan(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
{
	// get target player
	uint iShip;
	pub::Player::GetShip(iClientID, iShip);
	if (!iShip)
	{
		PrintUserCmdText(iClientID, L"ERR: You are not in space.");
		return true;
	}

	uint iTargetShip;
	pub::SpaceObj::GetTarget(iShip, iTargetShip);
	if (!iTargetShip)
	{
		PrintUserCmdText(iClientID, L"ERR: No selected target.");
		return true;
	}

	uint iClientIDTarget = HkGetClientIDByShip(iTargetShip);
	if (!iClientIDTarget)
	{
		PrintUserCmdText(iClientID, L"ERR: This is not a player ship.");
		return true;
	}

	// id
	const Archetype::Tractor* itemPtr = nullptr;
	auto& equipDesc = Players[iClientIDTarget].equipDescList.equip;
	list<EquipDesc>::iterator equip;
	for (equip = equipDesc.begin(); equip != equipDesc.end(); equip++)
	{
		if (equip->bMounted)
		{
			itemPtr = dynamic_cast<Archetype::Tractor*>(Archetype::GetEquipment(equip->iArchID));
			if (itemPtr)
			{
				break;
			}
		}
	}

	wstring wscTargetID = L"No mounted ID!";
	if (itemPtr != nullptr)
	{
		const GoodInfo* idInfo = GoodList_get()->find_by_archetype(itemPtr->get_id());
		if (idInfo == nullptr) // maybe a funny developer put one in the game as equipment but not a good?
		{
			wscTargetID = L"Unknown " + to_wstring(equip->iArchID);
		}
		else
		{
			wscTargetID = HkGetWStringFromIDS(idInfo->iIDSName);
		}
	}

	// ship
	const GoodInfo* shipInfo = GoodList_get()->find_by_ship_arch(Players[iClientIDTarget].iShipArchetype);
	wstring wscTargetShip = L"Unknown " + to_wstring(Players[iClientIDTarget].iShipArchetype);
	if (shipInfo != nullptr)
	{
		wscTargetShip = HkGetWStringFromIDS(shipInfo->iIDSName);
	}

	// affiliation
	int iTargetRep;
	uint uTargetAffiliation;
	pub::SpaceObj::GetRep(iTargetShip, iTargetRep);
	Reputation::Vibe::GetAffiliation(iTargetRep, uTargetAffiliation, false);
	wstring wscTargetAffiliation = HkGetWStringFromIDS(Reputation::get_name(uTargetAffiliation));

	// name
	wstring wscTargetCharName = (const wchar_t*)Players.GetActiveCharacterName(iClientIDTarget);

	// output
	PrintUserCmdText(iClientID, wscTargetCharName + L" - " + wscTargetShip);
	PrintUserCmdText(iClientID, wscTargetID + L" - " + wscTargetAffiliation);
	PrintUserCmdText(iClientID, L"Finished bounty scan at " + GetTimeString(false));
	return true;
}

typedef bool(*_UserCmdProc)(uint, const wstring &, const wstring &, const wchar_t*);

struct USERCMD
{
	wchar_t *wszCmd;
	_UserCmdProc proc;
	wchar_t *usage;
};

USERCMD UserCmds[] =
{
	{ L"/bountyscan", UserCmd_BountyScan, L"Usage: /bountyscan or /bs" },
	{ L"/bs", UserCmd_BountyScan, L"Usage: /bountyscan or /bs" },
};

bool UserCmd_Process(uint iClientID, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	wstring wscCmdLineLower = ToLower(wscCmd);

	for (uint i = 0; (i < sizeof(UserCmds) / sizeof(USERCMD)); i++)
	{

		if (wscCmdLineLower.find(UserCmds[i].wszCmd) == 0)
		{
			wstring wscParam = L"";
			if (wscCmd.length() > wcslen(UserCmds[i].wszCmd))
			{
				if (wscCmd[wcslen(UserCmds[i].wszCmd)] != ' ')
					continue;
				wscParam = wscCmd.substr(wcslen(UserCmds[i].wszCmd) + 1);
			}

			if (UserCmds[i].proc(iClientID, wscCmd, wscParam, UserCmds[i].usage))
			{
				returncode = SKIPPLUGINS_NOFUNCTIONCALL;
				return true;
			}
		}
	}
	return false;
}

EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "BountyScan by Alex";
	p_PI->sShortName = "bountyscan";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));

	return p_PI;
}
