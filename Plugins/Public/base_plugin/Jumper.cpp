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

void HyperJump::InitJumpHole(uint baseId, uint destSystem, uint destObject)
{
	uint dunno;
	IObjInspectImpl* inspect;
	GetShipInspect(baseId, inspect, dunno);	
	if (!inspect)
	{
		ConPrint(L"Something went very wrong!\n");
	}
	const CObject* solar = inspect->cobject();

	if (!solar)
	{
		ConPrint(L"Something went very wrong!\n");
	}

	memcpy((char*)solar + (0x6d * 4), &destSystem, 4);
	memcpy((char*)solar + (0x6e * 4), &destObject, 4);
}

void HyperJump::InitJumpHoleConfig()
{
	for (auto& base : player_bases)
	{
		PlayerBase* pbase = base.second;
		if (!mapArchs[pbase->basetype].isjump)
		{
			continue;
		}
		if (pub::SpaceObj::ExistsAndAlive(pbase->destObject) == -2) // method returns 0 for alive, -2 otherwise
		{
			wstring fileName = stows(pbase->path.substr(pbase->path.find_last_of('\\')+1));
			ConPrint(L"ERROR: Jump Base %ls's jump target does not exist, despawning it to prevent issues\nfilename: %ls\n", pbase->basename.c_str(), fileName.c_str());
			pbase->base_health = 0;
			CoreModule(pbase).SpaceObjDestroyed(CoreModule(pbase).space_obj, false);

			continue;
		}

		uint systemId;
		pub::SpaceObj::GetSystem(pbase->destObject, systemId);
		pbase->destSystem = systemId;

		pbase->Save();

		InitJumpHole(base.first, pbase->destSystem, pbase->destObject);
	}
}

void SetupCustomExitHole(PlayerBase* pb, SYSTEMJUMPCOORDS coords, uint coordsindex, uint exitJumpHoleLoadout, uint exitJumpHoleArchetype)
{
	auto systemInfo = Universe::get_system(coords.system);
	string baseNickName = "custom_jump_hole_exit_" + (string)systemInfo->nickname + "_" + to_string(coordsindex);

	if (pub::SpaceObj::ExistsAndAlive(CreateID(baseNickName.c_str())) == 0)
	{
		return;
	}

	SPAWN_SOLAR_STRUCT info;
	info.overwrittenName = L"Unstable Jump Hole Exit Point";
	info.iSystemId = coords.system;
	info.pos = coords.pos;
	info.ori = coords.ornt;
	info.nickname = baseNickName;
	info.loadoutArchetypeId = exitJumpHoleLoadout;
	info.solarArchetypeId = exitJumpHoleArchetype;
	info.solar_ids = 0;

	CreateSolar::CreateSolarCallout(&info);

	pb->destObject = info.iSpaceObjId;
	pb->destObjectName = baseNickName;
	pb->destSystem = coords.system;

	uint dunno;
	IObjInspectImpl* inspect;
	GetShipInspect(info.iSpaceObjId, inspect, dunno);
	const CObject* solar = inspect->cobject();

	memcpy((char*)solar + (0x6d * 4), &pb->destSystem, 4);
	memcpy((char*)solar + (0x6e * 4), &pb->destObject, 4);

	customSolarList.insert(info.iSpaceObjId);
}

void HyperJump::LoadHyperspaceHubConfig(const string& configPath) {

	string cfg_filejumpMap = configPath + "\\flhook_plugins\\jump_allowedsystems.cfg";
	string cfg_filehyperspaceHub = configPath + "\\flhook_plugins\\base_hyperspacehub.cfg";
	string cfg_filehyperspaceHubTimer = configPath + "\\flhook_plugins\\base_hyperspacehubtimer.cfg";
	vector<uint> legalReturnSystems;
	vector<uint> returnJumpHoles;
	vector<uint> hubToUnchartedJumpHoles;
	vector<uint> unchartedToHubJumpHoles;
	map<uint, wstring> systemNameMap;
	static map<uint, vector<SYSTEMJUMPCOORDS>> mapSystemJumps;
	uint lastJumpholeRandomization = 0;
	uint randomizationCooldown = 3600 * 23;
	uint exitJumpHoleArchetype = CreateID("flhook_jumphole");
	uint exitJumpHoleLoadout = CreateID("wormhole_unstable");
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

	ConPrint(L"HYPERSPACE HUB: Randomizing hub jump holes\n");

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
			if (ini.is_header("general"))
			{
				while (ini.read_value())
				{	
					if (ini.is_value("exitJumpHoleLoadout"))
					{
						exitJumpHoleLoadout = CreateID(ini.get_value_string(0));
					}
					else if (ini.is_value("exitJumpHoleArchetype"))
					{
						exitJumpHoleArchetype = CreateID(ini.get_value_string(0));
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
						string nickname = ini.get_value_string(0);
						uint nicknameHash = CreateID(nickname.c_str());
						if (!player_bases.count(nicknameHash))
						{
							ConPrint(L"HYPERSPACE HUB: Warning! Return jumphole %s not found, check config!\n", stows(nickname).c_str());
							continue;
						}
						returnJumpHoles.push_back(nicknameHash);
					}
				}
			}
			else if (ini.is_header("uncharted_return_jump_bases"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("base"))
					{
						string nickname = ini.get_value_string(0);
						uint nicknameHash = CreateID(nickname.c_str());
						if (!player_bases.count(nicknameHash))
						{
							ConPrint(L"HYPERSPACE HUB: Warning! Uncharted to Hub jumphole %s not found, check config!\n", stows(nickname).c_str());
							continue;
						}
						unchartedToHubJumpHoles.push_back(nicknameHash);
					}
				}
			}
			else if (ini.is_header("uncharted_jump_bases"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("base"))
					{
						string nickname = ini.get_value_string(0);
						uint nicknameHash = CreateID(nickname.c_str());
						if (!player_bases.count(nicknameHash))
						{
							ConPrint(L"HYPERSPACE HUB: Warning! Hub to Uncharted jumphole %s not found, check config!\n", stows(nickname).c_str());
							continue;
						}
						hubToUnchartedJumpHoles.push_back(nicknameHash);
					}
				}
			}
		}

		ini.close();
	}

	if (returnJumpHoles.size() > legalReturnSystems.size()
		|| hubToUnchartedJumpHoles.size() > unchartedToHubJumpHoles.size()) {
		ConPrint(L"HYPERSPACE HUB: ERROR! more random jump bases than distinct available destinations, aborting randomization!\n");
		return;
	}

	for (uint returnJH : returnJumpHoles) {

		PlayerBase* pb = player_bases[returnJH];
		uint index = rand() % legalReturnSystems.size();
		if (mapSystemJumps.count(legalReturnSystems.at(index)) == 0) {
			ConPrint(L"HYPERSPACE HUB: Jump Point data for return system not found, aborting randomization!\n");
			continue;
		}
		const auto& coordsList = mapSystemJumps[legalReturnSystems.at(index)];
		uint coordsIndex = rand() % coordsList.size();
		const auto& coords = coordsList.at(rand() % coordsList.size());

		SetupCustomExitHole(pb, coords, coordsIndex+1, exitJumpHoleLoadout, exitJumpHoleArchetype);

		const auto& systemInfo = Universe::get_system(coords.system);
		pb->basename = HkGetWStringFromIDS(systemInfo->strid_name) + L" Jump Hole";
		legalReturnSystems.erase(legalReturnSystems.begin() + index);

		pb->Save();
		RespawnBase(pb);
	}

	for (uint unchartedJH : hubToUnchartedJumpHoles) {
		PlayerBase* pb = player_bases[unchartedJH];
		uint randomizedIndex = rand() % unchartedToHubJumpHoles.size();
		uint randomizedTarget = unchartedToHubJumpHoles.at(randomizedIndex);
		auto targetJumpHole = player_bases.at(randomizedTarget);

		pb->destObject = randomizedTarget;

		auto unchartedSystemInfo = Universe::get_system(targetJumpHole->system);
		pb->basename = L"Unstable " + HkGetWStringFromIDS(unchartedSystemInfo->strid_name) + L" Jump Hole";


		targetJumpHole->destObject = unchartedJH;

		auto originSystemInfo = Universe::get_system(targetJumpHole->system);
		targetJumpHole->basename = L"Unstable " + HkGetWStringFromIDS(originSystemInfo->strid_name) + L" Jump Hole";


		unchartedToHubJumpHoles.erase(unchartedToHubJumpHoles.begin() + randomizedIndex);

		pb->Save();
		targetJumpHole->Save();
		RespawnBase(pb);
		RespawnBase(targetJumpHole);
	}

	WritePrivateProfileString("Timer", "lastRandomization", itos((int)currTime).c_str(), cfg_filehyperspaceHubTimer.c_str());
}