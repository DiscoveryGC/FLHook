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

void SetReturnHole(PlayerBase* originBase);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Settings Loading
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HyperJump::LoadHyperspaceHubConfig(const string& configPath) {

	string cfg_filejumpMap = configPath + "\\flhook_plugins\\jump_allowedsystems.cfg";
	string cfg_filehyperspaceHub = configPath + "\\flhook_plugins\\base_hyperspacehub.cfg";
	string cfg_filehyperspaceHubTimer = configPath + "\\flhook_plugins\\base_hyperspacehubtimer.cfg";
	vector<uint> legalReturnSystems;
	vector<uint> unchartedSystems;
	vector<uint> returnJumpHoles;
	vector<uint> unchartedJumpHoles;
	map<uint, wstring> systemNameMap;
	static map<uint, vector<SYSTEMJUMPCOORDS>> mapSystemJumps;
	uint lastJumpholeRandomization = 0;
	uint randomizationCooldown = 3600 * 23;
	INI_Reader ini;

	if (ini.open(cfg_filehyperspaceHubTimer.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("Timer")) {
				while (ini.read_value())
				{
					if (ini.is_value("lastRandomization"))
					{
						lastJumpholeRandomization = ini.get_value_int(0);
					}
					if (ini.is_value("randomizationCooldown"))
					{
						randomizationCooldown = ini.get_value_int(0);
					}
				}
			}
		}

		ini.close();
	}

	time_t currTime = time(0);
	if (lastJumpholeRandomization + randomizationCooldown > currTime)
	{
		ConPrint(L"HYPERSPACE HUB: insufficient time passed, aborting randomization\n");
		return;
	}

	ConPrint(L"HYPERSPACE HUB: Randomizing jump holes");

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
		ConPrint(L"HYPERSPACE HUB: ERROR! more random jump bases than distinct available destinations, aborting randomization!\n");
		return;
	}

	for (uint returnJH : returnJumpHoles) {
		if (!player_bases.count(returnJH)) {
			ConPrint(L"HYPERSPACE HUB: Warning! Return wormhole-base hash %u not found, check config!\n", returnJH);
			continue;
		}
		PlayerBase* pb = player_bases[returnJH];
		uint index = rand() % legalReturnSystems.size();
		if (mapSystemJumps.count(legalReturnSystems.at(index)) == 0) {
			ConPrint(L"HYPERSPACE HUB: Jump Point data for return system not found, aborting randomization!\n");
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
			ConPrint(L"HYPERSPACE HUB: Warning! Uncharted wormhole-base hash %u not found, check config!\n", unchartedJH);
			continue;
		}
		PlayerBase* pb = player_bases[unchartedJH];
		const uint index = rand() % unchartedSystems.size();
		if (mapSystemJumps.count(unchartedSystems.at(index)) == 0) {
			ConPrint(L"HYPERSPACE HUB: Jump Point data for uncharted system not found, aborting randomization!\n");
			continue;
		}
		const auto& coordsList = mapSystemJumps[unchartedSystems.at(index)];
		const auto& coords = coordsList.at(rand() % coordsList.size());
		pb->destsystem = coords.system;
		pb->destposition = coords.pos;
		pb->destorientation = coords.ornt;
		const auto& systemInfo = Universe::get_system(coords.system);
		pb->basename = L"Unstable " + HkGetWStringFromIDS(systemInfo->strid_name) + L" Jump Hole";
		unchartedSystems.erase(unchartedSystems.begin() + index);

		SetReturnHole(pb);
		pb->Save();
		RespawnBase(pb);
	}

	WritePrivateProfileString("Timer", "lastRandomization", itos((int)currTime).c_str(), cfg_filehyperspaceHubTimer.c_str());
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
	targetJumpHoleBase->basename = L"Unstable " + HkGetWStringFromIDS(systemInfo->strid_name) + L" Jump Hole";
	targetJumpHoleBase->destposition = originBase->position;
	targetJumpHoleBase->destorientation = originBase->rotation;

	targetJumpHoleBase->Save();
	RespawnBase(targetJumpHoleBase);
}
