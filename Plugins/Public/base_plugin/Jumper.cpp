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
#include <unordered_set>

#include <PluginUtilities.h>
#include "Main.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Structures and shit yo
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

unordered_set<uint> unchartedSystems;
uint unchartedSystemToExclude;

struct SYSTEMJUMPCOORDS
{
	uint system;
	Vector pos;
	Matrix ornt;
};

void HyperJump::CheckForUnchartedDisconnect(uint client, uint ship)
{
	if (unchartedSystems.count(Players[client].iSystemID))
	{
		pub::SpaceObj::SetRelativeHealth(ship, 0.0f);
	}
}

void HyperJump::InitJumpHole(uint baseId, uint destSystem, uint destObject)
{
	uint dunno;
	IObjInspectImpl* inspect;
	GetShipInspect(baseId, inspect, dunno);
	const CObject* solar = inspect->cobject();

	memcpy((uint*)solar + 0x6d, &destSystem, 4);
	memcpy((uint*)solar + 0x6e, &destObject, 4);
}

bool SetupCustomExitHole(PlayerBase* pb, SYSTEMJUMPCOORDS& coords, uint exitJumpHoleLoadout, uint exitJumpHoleArchetype)
{
	static uint counter = 0;
	auto systemInfo = Universe::get_system(coords.system);
	if (!systemInfo)
	{
		return false;
	}
	string baseNickName = "custom_return_hole_exit_" + (string)systemInfo->nickname + itos(counter);
	counter++;

	if (pub::SpaceObj::ExistsAndAlive(CreateID(baseNickName.c_str())) == 0) //0 means alive, -2 dead
	{
		return false;
	}

	SPAWN_SOLAR_STRUCT info;
	info.iSystemId = coords.system;
	info.pos = coords.pos;
	info.ori = coords.ornt;
	info.nickname = baseNickName;
	info.loadoutArchetypeId = exitJumpHoleLoadout;
	info.solarArchetypeId = exitJumpHoleArchetype;
	info.solar_ids = 267199;

	CreateSolar::CreateSolarCallout(&info);

	pb->destObject = info.iSpaceObjId;
	pb->destObjectName = baseNickName;
	pb->destSystem = coords.system;

	uint dunno;
	IObjInspectImpl* inspect;
	GetShipInspect(info.iSpaceObjId, inspect, dunno);
	const CObject* solar = inspect->cobject();

	memcpy((uint*)solar + 0x6d, &pb->destSystem, 4);
	memcpy((uint*)solar + 0x6e, &pb->destObject, 4);

	customSolarList.insert(info.iSpaceObjId);
	AddLog("ReturnJH to %s created\n", ((string)systemInfo->nickname).c_str());
	return true;
}

void HyperJump::InitJumpHoleConfig()
{
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string cfg_filehyperspaceHub = (string)szCurDir + R"(\flhook_plugins\base_hyperspacehub.cfg)";
	uint exitJumpHoleArchetype = CreateID("jumphole_noentry");
	uint exitJumpHoleLoadout = CreateID("wormhole_unstable");
	exitJumpHoleArchetype = CreateID(IniGetS(cfg_filehyperspaceHub, "general", "exitJumpHoleArchetype", "jumphole_noentry").c_str());
	exitJumpHoleLoadout = CreateID(IniGetS(cfg_filehyperspaceHub, "general", "exitJumpHoleLoadout", "wormhole_unstable").c_str());

	vector<PlayerBase*> invalidJumpHoles;
	for (auto& base : player_bases)
	{
		bool completedLoad = false;
		PlayerBase* pbase = base.second;
		if (!mapArchs[pbase->basetype].isjump)
		{
			continue;
		}

		if (mapArchs[pbase->basetype].ishubreturn)
		{
			SYSTEMJUMPCOORDS coords = { pbase->destSystem, pbase->destPos, pbase->destOri };
			completedLoad = SetupCustomExitHole(pbase, coords, exitJumpHoleLoadout, exitJumpHoleArchetype);
		}
		else if (pub::SpaceObj::ExistsAndAlive(pbase->destObject) == 0) // method returns 0 for alive, -2 otherwise
		{
			completedLoad = true;
		}
		
		if (!completedLoad)
		{
			invalidJumpHoles.emplace_back(pbase);
			continue;
		}

		uint systemId;
		pub::SpaceObj::GetSystem(pbase->destObject, systemId);
		pbase->destSystem = systemId;

		pbase->Save();

		InitJumpHole(base.first, pbase->destSystem, pbase->destObject);
	}

	for (auto pbase : invalidJumpHoles)
	{
		wstring fileName = stows(pbase->path.substr(pbase->path.find_last_of('\\') + 1));
		ConPrint(L"ERROR: Jump Base %ls's jump target/target system does not exist, despawning it to prevent issues\ntargetObject: %u, targetSystem: %u\nfilename: %ls\n", stows(pbase->nickname).c_str(), pbase->destObject, pbase->destSystem, fileName.c_str());
		pbase->base_health = 0;
		CoreModule(pbase).SpaceObjDestroyed(CoreModule(pbase).space_obj, false, false);
	}
}

void HyperJump::LoadHyperspaceHubConfig(const string& configPath)
{

	string cfg_filejumpMap = configPath + R"(\flhook_plugins\jump_allowedsystems.cfg)";
	string cfg_filehyperspaceHub = configPath + R"(\flhook_plugins\base_hyperspacehub.cfg)";
	string cfg_filehyperspaceHubTimer = configPath + R"(\flhook_plugins\base_hyperspacehubtimer.cfg)";
	vector<uint> legalReturnSystems;
	vector<uint> returnJumpHoles;
	vector<uint> hubToUnchartedJumpHoles;
	vector<uint> unchartedToHubJumpHoles;
	static map<uint, vector<SYSTEMJUMPCOORDS>> mapSystemJumps;
	uint lastJumpholeRandomization = 0;
	uint randomizationCooldown = 3600 * 23;
	uint randomizationCooldownOffset = 3600 * 9;
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
					if (ini.is_value("randomizationCooldownOffset"))
					{
						randomizationCooldownOffset = ini.get_value_int(0);
					}
					if (ini.is_value("systemToExclude"))
					{
						unchartedSystemToExclude = CreateID(ini.get_value_string(0));
					}
				}
			}
			else if (ini.is_header("uncharted_systems"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("system"))
					{
						unchartedSystems.insert(CreateID(ini.get_value_string(0)));
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
			if (ini.is_header("return_system_data"))
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
						if (player_bases.at(nicknameHash)->system == unchartedSystemToExclude)
						{
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
		|| hubToUnchartedJumpHoles.size() > unchartedToHubJumpHoles.size())
	{
		ConPrint(L"HYPERSPACE HUB: ERROR! more random jump bases than distinct available destinations, aborting randomization!\n");
		return;
	}

	for (uint returnJH : returnJumpHoles)
	{

		PlayerBase* pb = player_bases[returnJH];
		uint index = rand() % legalReturnSystems.size();
		if (mapSystemJumps.count(legalReturnSystems.at(index)) == 0)
		{
			ConPrint(L"HYPERSPACE HUB: Jump Point data for return system not found, aborting randomization!\n");
			continue;
		}
		const auto& coordsList = mapSystemJumps[legalReturnSystems.at(index)];
		const auto& coords = coordsList.at(rand() % coordsList.size());

		pb->destSystem = coords.system;
		pb->destPos = coords.pos;
		pb->destOri = coords.ornt;

		const auto& systemInfo = Universe::get_system(coords.system);
		pb->basename = HkGetWStringFromIDS(systemInfo->strid_name) + L" Jump Hole";
		legalReturnSystems.erase(legalReturnSystems.begin() + index);

		pb->Save();
		RespawnBase(pb);
	}

	bool isFirst = true;
	for (uint unchartedJH : hubToUnchartedJumpHoles)
	{
		PlayerBase* originJumpHole = player_bases[unchartedJH];
		uint randomizedIndex = rand() % unchartedToHubJumpHoles.size();
		uint randomizedTarget = unchartedToHubJumpHoles.at(randomizedIndex);
		auto targetJumpHole = player_bases.at(randomizedTarget);

		originJumpHole->destObject = CreateID(targetJumpHole->nickname.c_str());
		originJumpHole->destObjectName = targetJumpHole->nickname;
		originJumpHole->destSystem = targetJumpHole->system;

		auto unchartedSystemInfo = Universe::get_system(targetJumpHole->system);
		originJumpHole->basename = L"Unstable " + HkGetWStringFromIDS(unchartedSystemInfo->strid_name) + L" Jump Hole";

		targetJumpHole->destObject = CreateID(originJumpHole->nickname.c_str());
		targetJumpHole->destObjectName = originJumpHole->nickname;
		targetJumpHole->destSystem = originJumpHole->system;

		auto& selectedSystemCoordList = mapSystemJumps[targetJumpHole->system];
		auto& coords = selectedSystemCoordList.at(rand() % selectedSystemCoordList.size());
		targetJumpHole->position = coords.pos;
		targetJumpHole->rotation = coords.ornt;

		auto originSystemInfo = Universe::get_system(originJumpHole->system);
		targetJumpHole->basename = L"Unstable " + HkGetWStringFromIDS(originSystemInfo->strid_name) + L" Jump Hole";


		unchartedToHubJumpHoles.erase(unchartedToHubJumpHoles.begin() + randomizedIndex);

		originJumpHole->Save();
		targetJumpHole->Save();
		RespawnBase(originJumpHole);
		RespawnBase(targetJumpHole);

		if (isFirst)
		{
			isFirst = false;
			auto systemInfo = Universe::get_system(originJumpHole->destSystem);
			WritePrivateProfileStringA("Timer", "systemToExclude", systemInfo->nickname, cfg_filehyperspaceHubTimer.c_str());
		}
	}

	WritePrivateProfileString("Timer", "lastRandomization", itos((int)(currTime - (currTime % randomizationCooldown) + randomizationCooldownOffset)).c_str(), cfg_filehyperspaceHubTimer.c_str());
}