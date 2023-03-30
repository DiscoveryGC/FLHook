// AlleyPlugin for FLHookPlugin
// January 2015 by Alley
//
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
#include <float.h>
#include <FLHook.h>
#include <plugin.h>
#include <list>
#include <set>

#include <PluginUtilities.h>
#include "Main.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Structures and shit yo
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct SYSTEMJUMPCOORDS
{
	uint system;
	Vector pos;
	Matrix ornt;
};
static map<uint, vector<SYSTEMJUMPCOORDS>> mapSystemJumps;

static map<uint, SYSTEMJUMPCOORDS> mapDeferredJumps;

void SetReturnHole(PlayerBase* originBase);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Settings Loading
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HyperJump::LoadHyperspaceHubConfig(const string& configPath) {

	string cfg_filejumpMap = configPath + "\\flhook_plugins\\jump_allowedsystems.cfg";
	string cfg_filehyperspaceHub = configPath + "\\flhook_plugins\\base_hyperspacehub.cfg";
	vector<uint> legalReturnSystems;
	vector<uint> unchartedSystems;
	vector<uint> returnJumpHoles;
	vector<uint> unchartedJumpHoles;
	map<uint, wstring> systemNameMap;
	INI_Reader ini;

	if (ini.open(cfg_filejumpMap.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("system_jump_positions")) {
				while (ini.read_value())
				{
					if (ini.is_value("jump_position"))
					{
						SYSTEMJUMPCOORDS coords;
						coords.system = CreateID(ini.get_value_string(0));
						coords.pos = { ini.get_value_float(1), ini.get_value_float(2), ini.get_value_float(3) };

						Vector erot = { ini.get_value_float(4), ini.get_value_float(5), ini.get_value_float(6) };
						coords.ornt = EulerMatrix(erot);

						mapSystemJumps[coords.system].push_back(coords);
					}
				}
			}
		}

		ini.close();
	}

	if (ini.open(cfg_filehyperspaceHub.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("uncharted_system_data"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("uncharted_system"))
					{
						unchartedSystems.push_back(CreateID(ini.get_value_string(0)));
					}
				}
			}
			else if (ini.is_header("return_system_data"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("return_system"))
					{
						legalReturnSystems.push_back(CreateID(ini.get_value_string(0)));
					}
				}
			}
			else if (ini.is_header("return_jump_bases"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("base"))
					{
						returnJumpHoles.push_back(CreateID(ini.get_value_string(0)));
					}
				}
			}
			else if (ini.is_header("uncharted_jump_bases"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("base"))
					{
						unchartedJumpHoles.push_back(CreateID(ini.get_value_string(0)));
					}
				}
			}
		}

		ini.close();
	}

	if (returnJumpHoles.size() > legalReturnSystems.size()
	 || unchartedJumpHoles.size() > unchartedSystems.size()) {
		ConPrint(L"ERROR: more random jump bases than distinct available destinations, aborting randomization!\n");
		return;
	}

	for (uint returnJH : returnJumpHoles) {
		if (!player_bases.count(returnJH)) {
			ConPrint(L"Warning: Return wormhole-base hash %u not found, check config!\n", returnJH);
			continue;
		}
		PlayerBase* pb = player_bases[returnJH];
		uint index = rand() % legalReturnSystems.size();
		if (mapSystemJumps.count(legalReturnSystems.at(index)) == 0) {
			ConPrint(L"Jump Point data for return system not found, aborting randomization!\n");
			continue;
		}
		const auto& coordsList = mapSystemJumps[legalReturnSystems.at(index)];
		const auto& coords = coordsList.at(rand() % coordsList.size());
		pb->destsystem = coords.system;
		pb->destposition = coords.pos;
		pb->destorientation = coords.ornt;

		const auto& systemInfo = Universe::get_system(coords.system);
		pb->basename = HkGetWStringFromIDS(systemInfo->strid_name) + L" Jump Hole";
		legalReturnSystems.erase(legalReturnSystems.begin() + index);

		pb->Save();
		RespawnBase(pb);
	}

	for (uint unchartedJH : unchartedJumpHoles) {
		if (!player_bases.count(unchartedJH)) {
			ConPrint(L"Warning: Uncharted wormhole-base hash %u not found, check config!\n", unchartedJH);
			continue;
		}
		PlayerBase* pb = player_bases[unchartedJH];
		const uint index = rand() % unchartedSystems.size();
		if (mapSystemJumps.count(unchartedSystems.at(index)) == 0) {
			ConPrint(L"Jump Point data for uncharted system not found, aborting randomization!\n");
			continue;
		}
		const auto& coordsList = mapSystemJumps[unchartedSystems.at(index)];
		const auto& coords = coordsList.at(rand() % coordsList.size());
		pb->destsystem = coords.system;
		pb->destposition = coords.pos;
		pb->destorientation = coords.ornt;
		const auto& systemInfo = Universe::get_system(coords.system);
		pb->basename = HkGetWStringFromIDS(systemInfo->strid_name) + L" Jump Hole";
		unchartedSystems.erase(unchartedSystems.begin() + index);

		SetReturnHole(pb);
		pb->Save();
		RespawnBase(pb);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Dependencies
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SetReturnHole(PlayerBase* originBase) {
	const auto& targetSystem = Universe::get_system(originBase->destsystem);
	string targetJumpBaseNickname = "pb_hyperspace_hub_";
	targetJumpBaseNickname += reinterpret_cast<const char*>(targetSystem->nickname);
	targetJumpBaseNickname = ToLower(targetJumpBaseNickname);
	uint targetJumpHoleBaseHash = CreateID(ToLower(targetJumpBaseNickname).c_str());
	if (!player_bases.count(targetJumpHoleBaseHash)) {
		ConPrint(L"HYPERSPACEHUB: Error! Target base %ls not found!\n", stows(targetJumpBaseNickname).c_str());
		return;
	}
	auto targetJumpHoleBase = player_bases[targetJumpHoleBaseHash];

	const auto& systemInfo = Universe::get_system(originBase->system);
	targetJumpHoleBase->position = originBase->destposition;
	targetJumpHoleBase->destorientation = originBase->destorientation;
	targetJumpHoleBase->system = originBase->destsystem;
	targetJumpHoleBase->destsystem = originBase->system;
	targetJumpHoleBase->basename = HkGetWStringFromIDS(systemInfo->strid_name) + L" Jump Hole";
	targetJumpHoleBase->destposition = originBase->position;
	targetJumpHoleBase->destorientation = originBase->rotation;

	targetJumpHoleBase->Save();
	RespawnBase(targetJumpHoleBase);
}

void HyperJump::SwitchSystem(uint iClientID, uint system, Vector pos, Matrix ornt)
{
	mapDeferredJumps[iClientID].system = system;
	mapDeferredJumps[iClientID].pos = pos;
	mapDeferredJumps[iClientID].ornt = ornt;

	// Force a launch to put the ship in the right location in the current system so that
	// when the change system command arrives (hopefully) a fraction of a second later
	// the ship will appear at the right location.
	HkRelocateClient(iClientID, pos, ornt);
	// Send the jump command to the client. The client will send a system switch out complete
	// event which we intercept to set the new starting positions.
	PrintUserCmdText(iClientID, L" ChangeSys %u", system);
}

bool HyperJump::SystemSwitchOutComplete(unsigned int iShip, unsigned int iClientID)
{
	static PBYTE SwitchOut = 0;
	if (!SwitchOut)
	{
		SwitchOut = (PBYTE)hModServer + 0xf600;

		DWORD dummy;
		VirtualProtect(SwitchOut + 0xd7, 200, PAGE_EXECUTE_READWRITE, &dummy);
	}

	// Patch the system switch out routine to put the ship in a
	// system of our choosing.
	if (mapDeferredJumps.find(iClientID) != mapDeferredJumps.end())
	{
		uint iSystemID = mapDeferredJumps[iClientID].system;
		SwitchOut[0x0d7] = 0xeb;				// ignore exit object
		SwitchOut[0x0d8] = 0x40;
		SwitchOut[0x119] = 0xbb;				// set the destination system
		*(PDWORD)(SwitchOut + 0x11a) = iSystemID;
		SwitchOut[0x266] = 0x45;				// don't generate warning
		*(float*)(SwitchOut + 0x2b0) = mapDeferredJumps[iClientID].pos.z;		// set entry location
		*(float*)(SwitchOut + 0x2b8) = mapDeferredJumps[iClientID].pos.y;
		*(float*)(SwitchOut + 0x2c0) = mapDeferredJumps[iClientID].pos.x;
		*(float*)(SwitchOut + 0x2c8) = mapDeferredJumps[iClientID].ornt.data[2][2];
		*(float*)(SwitchOut + 0x2d0) = mapDeferredJumps[iClientID].ornt.data[1][1];
		*(float*)(SwitchOut + 0x2d8) = mapDeferredJumps[iClientID].ornt.data[0][0];
		*(float*)(SwitchOut + 0x2e0) = mapDeferredJumps[iClientID].ornt.data[2][1];
		*(float*)(SwitchOut + 0x2e8) = mapDeferredJumps[iClientID].ornt.data[2][0];
		*(float*)(SwitchOut + 0x2f0) = mapDeferredJumps[iClientID].ornt.data[1][2];
		*(float*)(SwitchOut + 0x2f8) = mapDeferredJumps[iClientID].ornt.data[1][0];
		*(float*)(SwitchOut + 0x300) = mapDeferredJumps[iClientID].ornt.data[0][2];
		*(float*)(SwitchOut + 0x308) = mapDeferredJumps[iClientID].ornt.data[0][1];
		*(PDWORD)(SwitchOut + 0x388) = 0x03ebc031;		// ignore entry object
		mapDeferredJumps.erase(iClientID);
		pub::SpaceObj::SetInvincible(iShip, false, false, 0);
		Server.SystemSwitchOutComplete(iShip, iClientID);
		SwitchOut[0x0d7] = 0x0f;
		SwitchOut[0x0d8] = 0x84;
		SwitchOut[0x119] = 0x87;
		*(PDWORD)(SwitchOut + 0x11a) = 0x1b8;
		*(PDWORD)(SwitchOut + 0x25d) = 0x1cf7f;
		SwitchOut[0x266] = 0x1a;
		*(float*)(SwitchOut + 0x2b0) =
			*(float*)(SwitchOut + 0x2b8) =
			*(float*)(SwitchOut + 0x2c0) = 0;
		*(float*)(SwitchOut + 0x2c8) =
			*(float*)(SwitchOut + 0x2d0) =
			*(float*)(SwitchOut + 0x2d8) = 1;
		*(float*)(SwitchOut + 0x2e0) =
			*(float*)(SwitchOut + 0x2e8) =
			*(float*)(SwitchOut + 0x2f0) =
			*(float*)(SwitchOut + 0x2f8) =
			*(float*)(SwitchOut + 0x300) =
			*(float*)(SwitchOut + 0x308) = 0;
		*(PDWORD)(SwitchOut + 0x388) = 0xcf8b178b;

		CUSTOM_JUMP_STRUCT info;
		info.iShipID = iShip;
		info.iSystemID = iSystemID;
		Plugin_Communication(CUSTOM_JUMP, &info);

		return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Logic
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HyperJump::ClearClientInfo(uint iClientID)
{
	mapDeferredJumps.erase(iClientID);
}
