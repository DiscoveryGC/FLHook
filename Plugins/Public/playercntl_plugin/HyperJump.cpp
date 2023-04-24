// Player Control plugin for FLHookPlugin
// Feb 2010 by Cannon
//
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

#include <windows.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <math.h>
#include <float.h>
#include <FLHook.h>
#include <plugin.h>
#include <math.h>
#include <list>
#include <set>
#include <algorithm>

#include <PluginUtilities.h>
#include "Main.h"
#include <hookext_exports.h>

#define RIGHT_CHECK(a) if(!(cmds->rights & a)) { cmds->Print(L"ERR No permission\n"); return; }

namespace HyperJump
{
	// Check that the item is a ship, cargo or equipment item is valid
	static uint CreateValidID(const char *nickname)
	{
		uint item = CreateID(nickname);

		if (!Archetype::GetEquipment(item)
			&& !Archetype::GetSimple(item)
			&& !Archetype::GetShip(item))
		{
			ConPrint(L"ERROR: item '%s' is not valid\n", stows(nickname).c_str());
		}

		return item;
	}

	// Ships restricted from jumping
	static set<uint> jumpRestrictedShipsList;

	static set<uint> setCloakingClients;

	void ClientCloakCallback(CLIENT_CLOAK_STRUCT* info)
	{
		if (info->isChargingCloak || info->isCloaked)
		{
			setCloakingClients.insert(info->iClientID);
		}
		else
		{
			setCloakingClients.erase(info->iClientID);
		}
	}

	static map<uint, map<uint, vector<uint>>> mapAvailableJumpSystems;

	static int JumpWhiteListEnabled = 0;
	static int JumpSystemListEnabled = 0;
	static uint BeaconCommodity = 0;
	static int BeaconTime = 120;
	static int BeaconCooldown = 300;
	static uint BeaconFuse = 0;
	static int JumpInnacuracy = 2000;
	static float JumpCargoSizeRestriction = 7000;
	static uint BlindJumpOverrideSystem = 0;
	static boolean CanJumpWithCommodities = true;
	static boolean CanGroupJumpWithCommodities = true;
	static boolean EnableFakeJumpTunnels = false;
	static uint BaseTunnelTransitTime = 10;

	struct JUMPFUSE
	{
		uint jump_fuse = 0;
		float lifetime = 0.0f;
		float delay = 0.0f;
	};
	
	// map<shipclass, map<JH/JD type, JUMPFUSE>> 
	static map<uint, map<JUMP_TYPE, JUMPFUSE>> JumpInFuseMap;

	struct SYSTEMJUMPCOORDS
	{
		uint system;
		wstring sector;
		Vector pos;
		Matrix ornt;
	};
	static map<uint, vector<SYSTEMJUMPCOORDS>> mapSystemJumps;
	static map<uint, SYSTEMJUMPCOORDS> mapDeferredJumps;
	static map<uint, JUMP_TYPE> mapJumpTypeOverride;

	struct JUMPDRIVE_ARCH
	{
		uint nickname;
		float can_jump_charge;
		float charge_rate;
		float discharge_rate;
		vector<uint> charge_fuse;
		uint jump_fuse;
		uint jump_range;
		map<uint, uint> mapFuelToUsage;
		float power;
		float field_range;
		float group_jump_range;
		boolean cd_disrupts_charge;
	};
	static map<uint, JUMPDRIVE_ARCH> mapJumpDriveArch;

	struct JUMPDRIVE
	{
		JUMPDRIVE_ARCH* arch = nullptr;

		bool charging_on;
		float curr_charge;
		uint active_fuse;
		float active_fuse_delay;
		list<uint> active_charge_fuse;
		bool charging_complete;
		uint charge_status;

		int jump_timer;
		int jump_tunnel_timer;
		uint iTargetSystem;
		Vector vTargetPosition;
		Matrix matTargetOrient;
	};
	static map<uint, JUMPDRIVE> mapJumpDrives;

	struct BEACONTIMER
	{
		int timeleft;
		int cooldown;
		bool decayed;
	};

	static map<uint, BEACONTIMER> mapActiveBeacons;

	struct BEACONMATRIX
	{
		uint nickname;
		float accuracy;
		uint range;
		uint item;
		int itemcount;
	};

	//There is only one kind of Matrix right now, but this might change later on
	static map<uint, BEACONMATRIX> mapBeaconMatrix;
	//map the existing Matrix
	static map<uint, BEACONMATRIX*> mapPlayerBeaconMatrix;

	void SwitchSystem(uint iClientID, uint system, Vector pos, Matrix ornt, uint tunnelTransitTime = BaseTunnelTransitTime)
	{
		if (EnableFakeJumpTunnels)
		{
			uint iSystem;
			uint iPlayerSystem;
			pub::Player::GetSystem(iClientID, iPlayerSystem);
			const auto& proxyJH = HkGetSystemNickByID(iPlayerSystem) + L"_proxy_jump_drive";
			uint ProxyJumpHoleID = CreateID(wstos(proxyJH).c_str());
			pub::SpaceObj::GetSystem(ProxyJumpHoleID, iSystem);

			if (iSystem != iPlayerSystem)
			{
				PrintUserCmdText(iClientID, L"ERR Jump failed, proxy jump hole %ls not located. Contact staff.", proxyJH.c_str());
				return;
			}
			uint playerShip;
			pub::Player::GetShip(iClientID, playerShip);
			FLPACKET_SYSTEM_SWITCH_OUT switchOutPacket;
			switchOutPacket.jumpObjectId = ProxyJumpHoleID;
			switchOutPacket.shipId = playerShip;
			HookClient->Send_FLPACKET_SERVER_SYSTEM_SWITCH_OUT(iClientID, switchOutPacket);

			mapJumpDrives[iClientID].jump_tunnel_timer = tunnelTransitTime;
		}
		else
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
	}

	void HyperJump::LoadSettings(const string &scPluginCfgFile)
	{
		// Patch Archetype::GetEquipment & Archetype::GetShip to suppress annoying warnings flserver-errors.log
		unsigned char patch1[] = { 0x90, 0x90 };
		WriteProcMem((char*)0x62F327E, &patch1, 2);
		WriteProcMem((char*)0x62F944E, &patch1, 2);
		WriteProcMem((char*)0x62F123E, &patch1, 2);

		char szCurDir[MAX_PATH];
		GetCurrentDirectory(sizeof(szCurDir), szCurDir);
		string scCfgFile = string(szCurDir) + "\\flhook_plugins\\jump.cfg";
		string scCfgFileSystemList = string(szCurDir) + "\\flhook_plugins\\jump_allowedsystems.cfg";

		int iItemID = 1;

		INI_Reader ini;
		if (ini.open(scCfgFile.c_str(), false))
		{
			while (ini.read_header())
			{
				if (ini.is_header("general"))
				{
					while (ini.read_value())
					{
						if (ini.is_value("JumpWhiteListEnabled"))
						{
							JumpWhiteListEnabled = ini.get_value_int(0);
							ConPrint(L"HYPERJUMP NOTICE: Ship Whitelist is %u (1=On, 0=Off)\n", JumpWhiteListEnabled);
						}
						else if (ini.is_value("JumpSystemListEnabled"))
						{
							JumpSystemListEnabled = ini.get_value_int(0);
							ConPrint(L"HYPERJUMP NOTICE: System Whitelist is %u (1=On, 0=Off)\n", JumpSystemListEnabled);
						}
						else if (ini.is_value("BeaconCommodity"))
						{
							BeaconCommodity = CreateID(ini.get_value_string());
						}
						else if (ini.is_value("BeaconTime"))
						{
							BeaconTime = ini.get_value_int(0);
						}
						else if (ini.is_value("BeaconFuse"))
						{
							BeaconFuse = CreateID(ini.get_value_string());
						}
						else if (ini.is_value("BeaconCooldown"))
						{
							BeaconCooldown = ini.get_value_int(0);
						}
						else if (ini.is_value("JumpInnacuracy"))
						{
							JumpInnacuracy = ini.get_value_int(0);
						}
						else if (ini.is_value("BlindJumpOverrideSystem")) {
							BlindJumpOverrideSystem = CreateID(ini.get_value_string());
						}
						else if (ini.is_value("CanJumpWithCommodities"))
						{
							CanJumpWithCommodities = ini.get_value_bool(0);
						}
						else if (ini.is_value("CanGroupJumpWithCommodities"))
						{
							CanGroupJumpWithCommodities = ini.get_value_bool(0);
						}
						else if (ini.is_value("EnableFakeJumpTunnels"))
						{
							EnableFakeJumpTunnels = ini.get_value_bool(0);
						}
						else if (ini.is_value("BaseTunnelTransitTime"))
						{
							BaseTunnelTransitTime = ini.get_value_int(0);
						}
						else if (ini.is_value("JumpInFuse"))
						{
							uint shipType = ini.get_value_int(0);
							JUMP_TYPE jumpType = static_cast<JUMP_TYPE>(ini.get_value_int(1));
							JUMPFUSE jumpFuse;
							jumpFuse.jump_fuse = CreateID(ini.get_value_string(2));
							jumpFuse.lifetime = ini.get_value_float(3);
							jumpFuse.delay = ini.get_value_float(4);
							JumpInFuseMap[shipType][jumpType] = jumpFuse;
						}
					}
				}
				else if (ini.is_header("shiprestrictions"))
				{
					while (ini.read_value())
					{
						if (ini.is_value("restrict"))
						{
							uint nicknameHash = CreateID(ini.get_value_string(0));
							jumpRestrictedShipsList.emplace(nicknameHash);
						}
						else if (ini.is_value("JumpCargoSizeRestriction"))
						{
							JumpCargoSizeRestriction = ini.get_value_float(0);
						}
					}
				}
				else if (ini.is_header("jumpdrive"))
				{
					JUMPDRIVE_ARCH jd;
					while (ini.read_value())
					{
						if (ini.is_value("nickname"))
						{
							jd.nickname = CreateValidID(ini.get_value_string(0));
						}
						else if (ini.is_value("can_jump_charge"))
						{
							jd.can_jump_charge = ini.get_value_float(0);
						}
						else if (ini.is_value("charge_rate"))
						{
							jd.charge_rate = ini.get_value_float(0);
						}
						else if (ini.is_value("discharge_rate"))
						{
							jd.discharge_rate = ini.get_value_float(0);
						}
						else if (ini.is_value("charge_fuse"))
						{
							jd.charge_fuse.push_back(CreateID(ini.get_value_string(0)));
						}
						else if (ini.is_value("jump_fuse"))
						{
							jd.jump_fuse = CreateID(ini.get_value_string(0));
						}
						else if (ini.is_value("jump_range"))
						{
							jd.jump_range = ini.get_value_int(0);
						}
						else if (ini.is_value("fuel"))
						{
							uint fuel = CreateValidID(ini.get_value_string(0));
							int rate = ini.get_value_int(1);
							jd.mapFuelToUsage[fuel] = rate;
						}
						else if (ini.is_value("power"))
						{
							jd.power = ini.get_value_float(0);
						}
						else if (ini.is_value("field_range"))
						{
							jd.field_range = ini.get_value_float(0);
						}
						else if (ini.is_value("group_jump_range"))
						{
							jd.group_jump_range = ini.get_value_float(0);
						}
						else if (ini.is_value("cd_disrupts_charge"))
						{
							jd.cd_disrupts_charge = ini.get_value_bool(0);
						}
					}
					mapJumpDriveArch[jd.nickname] = jd;
				}
				else if (ini.is_header("beacon"))
				{
					BEACONMATRIX bm;
					while (ini.read_value())
					{
						if (ini.is_value("nickname"))
						{
							bm.nickname = CreateValidID(ini.get_value_string(0));
						}
						else if (ini.is_value("accuracy"))
						{
							bm.accuracy = ini.get_value_float(0);
						}
						else if (ini.is_value("item"))
						{
							bm.item = CreateValidID(ini.get_value_string(0));
							bm.itemcount = ini.get_value_int(0);
						}
						else if (ini.is_value("range"))
						{
							bm.range = ini.get_value_int(0);
						}
					}
					mapBeaconMatrix[bm.nickname] = bm;
				}
			}
			if (BeaconCommodity == 0)
			{
				BeaconCommodity = CreateID("commodity_event_04");
			}
			if (BeaconFuse == 0)
			{
				BeaconFuse = CreateID("fuse_jumpdrive_charge_5");
			}
			ini.close();
		}

		if (ini.open(scCfgFileSystemList.c_str(), false))
		{
			uint sysConnectionListSize = 0;
			uint coordListSize = 0;
			while (ini.read_header())
			{
				if (ini.is_header("system"))
				{
					uint originSystem;
					uint jumpRange;
					vector<uint> targetSystemsList;
					while (ini.read_value())
					{
						if (ini.is_value("target_system"))
						{
							targetSystemsList.push_back(CreateID(ini.get_value_string(0)));
							sysConnectionListSize++;
						}
						else if (ini.is_value("origin_system"))
						{
							originSystem = CreateID(ini.get_value_string(0));
						}
						else if (ini.is_value("depth"))
						{
							jumpRange = ini.get_value_int(0);
						}
					}
					mapAvailableJumpSystems[originSystem][jumpRange] = targetSystemsList;
				}
				if (ini.is_header("system_jump_positions")) {
					while (ini.read_value())
					{
						if (ini.is_value("jump_position"))
						{
							SYSTEMJUMPCOORDS coords;
							coords.system = CreateID(ini.get_value_string(0));
							coords.pos = { ini.get_value_float(1), ini.get_value_float(2), ini.get_value_float(3) };

							Vector erot = {ini.get_value_float(4), ini.get_value_float(5), ini.get_value_float(6) };
							coords.ornt = EulerMatrix(erot);
							coords.sector = VectorToSectorCoord(coords.system, coords.pos);

							mapSystemJumps[coords.system].push_back(coords);
							coordListSize++;
						}
					}
				}
			}
			ConPrint(L"Hyperspace: Loaded %u system coordinates and %u connections for %u systems\n", coordListSize, sysConnectionListSize, mapAvailableJumpSystems.size());
			ini.close();
		}

		// Remove patch now that we've finished loading.
		unsigned char patch2[] = { 0xFF, 0x12 };
		WriteProcMem((char*)0x62F327E, &patch2, 2);
		WriteProcMem((char*)0x62F944E, &patch2, 2);
		WriteProcMem((char*)0x62F123E, &patch2, 2);

		ConPrint(L"Jumpdrive [%d]\n", mapJumpDriveArch.size());
		ConPrint(L"Beacon Matrix [%d]\n", mapBeaconMatrix.size());
	}

	void RandomizeCoords(Vector& vec) {
		vec.x += (rand() % (JumpInnacuracy * 2)) - JumpInnacuracy;
		vec.y += (rand() % (JumpInnacuracy * 2)) - JumpInnacuracy;
		vec.z += (rand() % (JumpInnacuracy * 2)) - JumpInnacuracy;
	}

	void SetFuse(uint iClientID, uint fuse, float lifetime = 0.0f, float delay = 0.0f)
	{
		JUMPDRIVE &jd = mapJumpDrives[iClientID];
		if (jd.active_fuse)
		{
			IObjInspectImpl *obj = HkGetInspect(iClientID);
			if (obj)
			{
				HkUnLightFuse((IObjRW*)obj, jd.active_fuse, jd.active_fuse_delay);
			}
			jd.active_fuse = 0;
		}

		if (fuse)
		{
			IObjInspectImpl *obj = HkGetInspect(iClientID);
			if (obj)
			{
				jd.active_fuse = fuse;
				jd.active_fuse_delay = delay;
				HkLightFuse((IObjRW*)obj, jd.active_fuse, delay, lifetime, 0.0f);
			}
		}
	}

	void HyperJump::SetJumpInFuse(uint iClientID, JUMP_TYPE jumpType)
	{
		//if incoming from a jumpdrive jump, overwrite the type
		if (mapJumpDrives[iClientID].jump_tunnel_timer)
			jumpType = JUMPDRIVE_JUMPTYPE;

		Archetype::Ship* victimShiparch = Archetype::GetShip(Players[iClientID].iShipArchetype);
		if (JumpInFuseMap.count(victimShiparch->iShipClass) && JumpInFuseMap[victimShiparch->iShipClass].count(jumpType))
		{
			const JUMPFUSE& jumpFuse = JumpInFuseMap[victimShiparch->iShipClass][jumpType];
			SetFuse(iClientID, jumpFuse.jump_fuse, jumpFuse.lifetime, jumpFuse.delay);
		}
		else
		{
			SetFuse(iClientID, 0);
		}
	}

	void AddChargeFuse(uint iClientID, uint fuse)
	{
		IObjInspectImpl *obj = HkGetInspect(iClientID);
		if (obj)
		{
			mapJumpDrives[iClientID].active_charge_fuse.push_back(fuse);
			HkLightFuse((IObjRW*)obj, fuse, 0, 0, 0);
		}
	}

	void StopChargeFuses(uint iClientID)
	{
		IObjInspectImpl *obj = HkGetInspect(iClientID);
		if (obj)
		{
			foreach(mapJumpDrives[iClientID].active_charge_fuse, uint, fuse)
				HkUnLightFuse((IObjRW*)obj, *fuse, 0);
			mapJumpDrives[iClientID].active_charge_fuse.clear();
		}
	}

	bool InitJumpDriveInfo(uint iClientID, bool fullCheck = false)
	{
		// Initialise the drive parameters for this ship
		if (mapJumpDrives.count(iClientID) && mapJumpDrives[iClientID].arch != nullptr && !fullCheck)
		{
			return true;
		}
		mapJumpDrives[iClientID].charging_on = false;
		mapJumpDrives[iClientID].curr_charge = 0;
		mapJumpDrives[iClientID].active_fuse = 0;
		mapJumpDrives[iClientID].active_charge_fuse.clear();
		mapJumpDrives[iClientID].charging_complete = false;
		mapJumpDrives[iClientID].charge_status = -1;

		mapJumpDrives[iClientID].jump_timer = 0;
		mapJumpDrives[iClientID].iTargetSystem = 0;
		mapJumpDrives[iClientID].vTargetPosition.x = 0;
		mapJumpDrives[iClientID].vTargetPosition.y = 0;
		mapJumpDrives[iClientID].vTargetPosition.z = 0;

		// Check that the player has a jump drive and initialise the infomation
		// about it - otherwise return false.
		for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
		{
			if (mapJumpDriveArch.find(item->iArchID) != mapJumpDriveArch.end())
			{
				if (item->bMounted)
				{
					mapJumpDrives[iClientID].arch = &mapJumpDriveArch[item->iArchID];
					return true;
				}
			}
		}
		return false;
	}

	bool CheckForBeacon(uint iClientID, bool fullCheck = false)
	{
		if (mapPlayerBeaconMatrix.count(iClientID) && !fullCheck) {
			return true;
		}
		for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
		{
			if (mapBeaconMatrix.find(item->iArchID) != mapBeaconMatrix.end())
			{
				mapPlayerBeaconMatrix[iClientID] = &mapBeaconMatrix[item->iArchID];
				return true;
			}
		}

		return false;
	}

	bool IsSystemJumpable(uint systemFrom, uint systemTo, uint range)
	{
		if (mapAvailableJumpSystems.count(systemFrom) == 0)
			return false;

		if (systemFrom == systemTo)
			return true;

		auto& allowedSystemsByRange = mapAvailableJumpSystems[systemFrom];
		for (uint depth = 1; depth <= range; depth++) {
			if (allowedSystemsByRange.count(range) && find(allowedSystemsByRange[depth].begin(), allowedSystemsByRange[depth].end(), systemTo) != allowedSystemsByRange[depth].end())
				return true;
		}
		return false;
	}

	bool CheckForCommodities(uint iClientID)
	{
		int iRemHoldSize;
		std::list<CARGO_INFO> lstCargo;
		HkEnumCargo(ARG_CLIENTID(iClientID), lstCargo, iRemHoldSize);
		for (auto& cargo : lstCargo) {
			bool flag = false;
			pub::IsCommodity(cargo.iArchID, flag);
			if (flag)
				return true;
		}
		return false;
	}

	bool HyperJump::UserCmd_CanBeaconJumpToPlayer(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
	{
		if (wscParam.empty()) {
			PrintUserCmdText(iClientID, L"ERR Invalid parameters");
			PrintUserCmdText(iClientID, usage);
			return true;
		}
		if (!InitJumpDriveInfo(iClientID)) {
			PrintUserCmdText(iClientID, L"ERR Jump Drive not equipped");
			return true;
		}
		wstring wscCharname = GetParam(wscParam, L' ', 0);
		uint iTargetClientID = HkGetClientIdFromCharname(wscCharname);
		if (iTargetClientID == UINT_MAX) {
			uint targetClientID = ToUInt(wscCharname);
			if (targetClientID && HkIsValidClientID(targetClientID))
			{
				iTargetClientID = targetClientID;
			}
			else 
			{
				PrintUserCmdText(iClientID, L"ERR Player not online");
				return true;
			}
		}

		if (!CheckForBeacon(iTargetClientID)) {
			PrintUserCmdText(iClientID, L"ERR Target has no beacon equipped");
			return true;
		}

		if (!JumpSystemListEnabled || IsSystemJumpable(Players[iClientID].iSystemID, Players[iTargetClientID].iSystemID, mapJumpDrives[iClientID].arch->jump_range + mapPlayerBeaconMatrix[iTargetClientID]->range))
		{
			PrintUserCmdText(iClientID, L"Player beacon in jump range");
		}
		else
		{
			PrintUserCmdText(iClientID, L"Player beacon out of jump range");
		}
		return true;
	}

	bool HyperJump::UserCmd_ListJumpableSystems(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
	{
		uint iSystemID;
		pub::Player::GetSystem(iClientID, iSystemID);
		const auto& playerJumpDrive = mapJumpDrives[iClientID];
		if (!InitJumpDriveInfo(iClientID)) {
			PrintUserCmdText(iClientID, L"ERR Jump Drive not equipped");
			return true;
		}

		if (JumpSystemListEnabled == 1)
		{
			if (mapAvailableJumpSystems.count(iSystemID) == 0) {
				PrintUserCmdText(iClientID, L"ERR Jumping from this system is not possible");
				return true;
			}

			PrintUserCmdText(iClientID, L"You are allowed to jump to:");
			auto& systemRangeList = mapAvailableJumpSystems[iSystemID];
			for (uint depth = 1; depth <= playerJumpDrive.arch->jump_range; depth++)
			{
				for (uint &allowed_sys : systemRangeList[depth])
				{
					const Universe::ISystem *systemData = Universe::get_system(allowed_sys);
					wstring wscSysNameList = HkGetWStringFromIDS(systemData->strid_name);
					PrintUserCmdText(iClientID, L"|     %s", wscSysNameList.c_str());
				}
			}
		}
		else
		{
			PrintUserCmdText(iClientID, L"Jump System Whitelisting is not enabled.");
		}
		return true;
	}

	bool UserCmd_ClearSystem(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage)
	{
		if (!InitJumpDriveInfo(iClientID)) {
			PrintUserCmdText(iClientID, L"ERR No jump drive installed");
			return true;
		}
		mapJumpDrives[iClientID].iTargetSystem = 0;
		PrintUserCmdText(iClientID, L"System coordinates cleared");
		return true;
	}

	bool HyperJump::UserCmd_IsSystemJumpable(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage) {
		if (!InitJumpDriveInfo(iClientID)) {
			PrintUserCmdText(iClientID, L"ERR No jump drive installed");
			return true;
		}
		wstring sysName = ToLower(GetParamToEnd(wscParam, ' ', 0));
		if (sysName.empty()) {
			PrintUserCmdText(iClientID, L"ERR Invalid system name");
			return true;
		}
		for (struct Universe::ISystem *sysinfo = Universe::GetFirstSystem(); sysinfo; sysinfo = Universe::GetNextSystem())
		{
			const auto& fullSystemName = HkGetWStringFromIDS(sysinfo->strid_name);
			if (ToLower(fullSystemName) == sysName) {
				uint &iTargetSystemID = sysinfo->id;
				uint iClientSystem;
				pub::Player::GetSystem(iClientID, iClientSystem);

				if (IsSystemJumpable(iClientSystem, iTargetSystemID, mapJumpDrives[iClientID].arch->jump_range)) {
					PrintUserCmdText(iClientID, L"%ls is within your jump range of %u systems", fullSystemName.c_str(), mapJumpDrives[iClientID].arch->jump_range);
				}
				else {
					PrintUserCmdText(iClientID, L"%ls is out of your jump range of %u systems", fullSystemName.c_str(), mapJumpDrives[iClientID].arch->jump_range);
				}
				return true;
			}
		}

		PrintUserCmdText(iClientID, L"ERR Invalid system name");
		return true;
	}
	
	bool UserCmd_SetSystem(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage)
	{
		if (!InitJumpDriveInfo(iClientID)) {
			PrintUserCmdText(iClientID, L"ERR Jump drive not equipped");
			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging_failed"));
			return true;
		}
		wstring sysName = ToLower(GetParamToEnd(wscParam, ' ', 0));
		if (sysName.empty()) {
			PrintUserCmdText(iClientID, L"ERR Invalid system name");
			return true;
		}
		for (struct Universe::ISystem *sysinfo = Universe::GetFirstSystem(); sysinfo; sysinfo = Universe::GetNextSystem())
		{
			const auto& fullSystemName = HkGetWStringFromIDS(sysinfo->strid_name);
			if (ToLower(fullSystemName) == sysName) {
				uint &iTargetSystemID = sysinfo->id;

				uint iPlayerSystem;
				pub::Player::GetSystem(iClientID, iPlayerSystem);
				if (!IsSystemJumpable(iPlayerSystem, iTargetSystemID, mapJumpDrives[iClientID].arch->jump_range)) {
					PrintUserCmdText(iClientID, L"System out of range, use /jumplist for a list of valid destinations");
					return true;
				}
				if (mapSystemJumps.count(iTargetSystemID) == 0) {
					PrintUserCmdText(iClientID, L"ERR Jumps to selected system not configured, please report the issue to the staff");
					return true;
				}
				SYSTEMJUMPCOORDS &jumpCoords = mapSystemJumps[iTargetSystemID].at(0);
				mapJumpDrives[iClientID].iTargetSystem = iTargetSystemID;
				mapJumpDrives[iClientID].vTargetPosition = jumpCoords.pos;
				mapJumpDrives[iClientID].matTargetOrient = jumpCoords.ornt;

				PrintUserCmdText(iClientID, L"System locked in, jumping to %ls, sector %ls", fullSystemName.c_str(), jumpCoords.sector.c_str());
				if (mapSystemJumps[iTargetSystemID].size() > 1) {
					PrintUserCmdText(iClientID, L"Alternate jump coordinates available, use /setsector to switch");
					int iCount = 1;
					for (auto coord : mapSystemJumps[iTargetSystemID]) {
						PrintUserCmdText(iClientID, L"%u. %ls", iCount, coord.sector.c_str());
						iCount++;
					}
				}
				return true;
			}
		}
		PrintUserCmdText(iClientID, L"ERR Invalid system name");
		return true;
	}

	bool UserCmd_SetSector(uint iClientID, const wstring & wscCmd, const wstring & wscParam, const wchar_t * usage)
	{
		if (!InitJumpDriveInfo(iClientID)) {
			PrintUserCmdText(iClientID, L"ERR Jump drive not equipped");
			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging_failed"));
			return true;
		}

		if (mapJumpDrives[iClientID].iTargetSystem == 0) 
		{
			PrintUserCmdText(iClientID, L"ERR Select system with /setsystem first!");
			return true;
		}

		auto &jd = mapJumpDrives[iClientID];
		uint index = ToUInt(GetParam(wscParam, ' ', 0));
		if (index == 0 || mapSystemJumps[jd.iTargetSystem].size() < index ) {
			PrintUserCmdText(iClientID, L"ERR invalid selection");
			return true;
		}
		auto &coords = mapSystemJumps[jd.iTargetSystem].at(index - 1);
		jd.vTargetPosition = coords.pos;
		jd.matTargetOrient = coords.ornt;

		PrintUserCmdText(iClientID, L"Sector %s selected", coords.sector.c_str());
		return true;
	}

	void HyperJump::Timer()
	{
		list<uint> lstOldClients;

		// Handle beacons
		for (map<uint, BEACONTIMER>::iterator i = mapActiveBeacons.begin(); i != mapActiveBeacons.end(); ++i)
		{
			BEACONTIMER &bc = i->second;

			if (!bc.decayed)
			{
				if (bc.timeleft <= 0)
				{
					IObjInspectImpl *obj = HkGetInspect(i->first);
					PrintUserCmdText(i->first, L"Hyperspace beacon has decayed.");
					bc.decayed = true;
					HkUnLightFuse((IObjRW*)obj, BeaconFuse, 0);
				}
				else
				{
					bc.timeleft -= 1;
				}
			}

			if (bc.cooldown == 0)
			{
				PrintUserCmdText(i->first, L"Hyperspace beacon cooldown complete.");
				mapActiveBeacons.erase(i->first);
			}
			else
			{
				bc.cooldown -= 1;
			}
		}

		lstOldClients.clear();

		// Handle jump drive charging
		for (map<uint, JUMPDRIVE>::iterator iter = mapJumpDrives.begin(); iter != mapJumpDrives.end(); iter++)
		{
			uint iClientID = iter->first;

			uint iShip;
			pub::Player::GetShip(iClientID, iShip);
			if (iShip == 0)
			{
				lstOldClients.push_back(iClientID);
			}
			else
			{
				JUMPDRIVE &jd = iter->second;

				if (jd.jump_tunnel_timer > 0)
				{
					jd.jump_tunnel_timer--;
					PrintUserCmdText(iClientID, L"Tunnel timer %d\n", jd.jump_tunnel_timer);
					if (jd.jump_tunnel_timer == 2)
					{
						// switch the system under the hood, final coordinates will be set by the packet next step.
						PrintUserCmdText(iClientID, L" ChangeSys %u", jd.iTargetSystem);
					}
					else if (jd.jump_tunnel_timer == 1)
					{
						FLPACKET_SYSTEM_SWITCH_IN switchInPacket;
						switchInPacket.objType = OBJ_JUMP_HOLE;
						switchInPacket.pos = jd.vTargetPosition;
						switchInPacket.quat = HkMatrixToQuaternion(jd.matTargetOrient);
						switchInPacket.shipId = iShip;
						HookClient->Send_FLPACKET_SERVER_SYSTEM_SWITCH_IN(iClientID, switchInPacket);
					}
				}

				if (jd.arch == nullptr) {
					continue;
				}
				if (jd.jump_timer > 0)
				{
					if (setCloakingClients.find(iClientID) != setCloakingClients.end())
					{
						PrintUserCmdText(iClientID, L"ERR Ship is cloaked.");
						SetFuse(iClientID, 0);
						StopChargeFuses(iClientID);
						jd.jump_timer = 0;
						continue;
					}

					jd.jump_timer--;
					// Turn on the jumpdrive flash
					if (jd.jump_timer == 7)
					{
						jd.charging_complete = false;
						jd.curr_charge = 0.0;
						jd.charging_on = false;
						SetFuse(iClientID, jd.arch->jump_fuse);
						pub::Audio::PlaySoundEffect(iClientID, CreateID("dsy_jumpdrive_activate"));
					}
					// Execute the jump and do the pop sound
					else if (jd.jump_timer == 2)
					{

						// Stop the charging fuses
						StopChargeFuses(iClientID);

						// Jump the player's ship
						Vector vPosition;
						Matrix mShipDir;
						pub::SpaceObj::GetLocation(iShip, vPosition, mShipDir);

						uint iSystemID;
						pub::SpaceObj::GetSystem(iShip, iSystemID);

						pub::SpaceObj::DrainShields(iShip);
						// Restrict some ships from jumping, this is for the jumpship
						auto shipInfo1 = Archetype::GetShip(Players[iClientID].iShipArchetype);
						if (!CanJumpWithCommodities && CheckForCommodities(iClientID))
						{
							jd.charging_complete = false;
							jd.curr_charge = 0.0;
							jd.charging_on = false;
							StopChargeFuses(iClientID);
							PrintUserCmdText(iClientID, L"ERR Jumping with commodities onboard is forbidden.");
							continue;
						}
						if (JumpCargoSizeRestriction <= shipInfo1->fHoldSize) {
							jd.charging_complete = false;
							jd.curr_charge = 0.0;
							jd.charging_on = false;
							StopChargeFuses(iClientID);
							PrintUserCmdText(iClientID, L"ERR Cargo hold too large, jumping forbidden");
							continue;
						}
						if (JumpWhiteListEnabled == 1 && jumpRestrictedShipsList.find(Players[iClientID].iShipArchetype) != jumpRestrictedShipsList.end())
						{
							jd.charging_complete = false;
							jd.curr_charge = 0.0;
							jd.charging_on = false;
							StopChargeFuses(iClientID);
							PrintUserCmdText(iClientID, L"ERR Ship is not allowed to jump.");
							continue;
						}
						
						RandomizeCoords(jd.vTargetPosition);
						SwitchSystem(iClientID, jd.iTargetSystem, jd.vTargetPosition, jd.matTargetOrient, BaseTunnelTransitTime);

						// Find all ships within the jump field including the one with the jump engine.
						if (jd.arch->field_range <= 0 && jd.arch->group_jump_range <= 0)
							continue;

						list<GROUP_MEMBER> lstGrpMembers;
						HkGetGroupMembers((const wchar_t*)Players.GetActiveCharacterName(iClientID), lstGrpMembers);

						uint tunnelTransitTime = BaseTunnelTransitTime;

						struct PlayerData *pPD = nullptr;
						while (pPD = Players.traverse_active(pPD))
						{
							uint iSystemID2;
							pub::SpaceObj::GetSystem(pPD->iShipID, iSystemID2);

							if (iSystemID2 != iSystemID || pPD->iOnlineID == iClientID)
								continue;

							Vector vPosition2;
							Matrix mShipDir2;
							pub::SpaceObj::GetLocation(pPD->iShipID, vPosition2, mShipDir2);

							float distance = HkDistance3D(vPosition, vPosition2);
							boolean isGroupMember = false;
							boolean inRange = false;
							for (const auto& member : lstGrpMembers)
							{
								if (member.iClientID == pPD->iOnlineID)
								{
									isGroupMember = true;
									break;
								}
							}

							if (isGroupMember)
							{
								inRange = distance <= jd.arch->group_jump_range;
							}
							else
							{
								inRange = distance <= jd.arch->jump_range;
							}

							if (!inRange)
								continue;
							// Restrict some ships from jumping, this is for the jumpers
							auto shipInfo2 = Archetype::GetShip(Players[iClientID].iShipArchetype);
							if (!CanJumpWithCommodities && CheckForCommodities(pPD->iOnlineID))
							{
								PrintUserCmdText(iClientID, L"ERR Jumping with commodities onboard is forbidden.");
								continue;
							}
							if (JumpCargoSizeRestriction <= shipInfo2->fHoldSize) {
								PrintUserCmdText(iClientID, L"ERR Cargo hold too large, jumping forbidden");
								continue;
							}
							if (JumpWhiteListEnabled == 1 && jumpRestrictedShipsList.find(Players[pPD->iOnlineID].iShipArchetype) != jumpRestrictedShipsList.end())
							{
								PrintUserCmdText(iClientID, L"ERR Ship is not allowed to jump.");
								continue;
							}
							PrintUserCmdText(pPD->iOnlineID, L"Jumping...");

							if (HookExt::IniGetB(iClientID, "event.enabled"))
							{
								string eventid = wstos(HookExt::IniGetWS(iClientID, "event.eventid"));

								//else disable event mode
								HookExt::IniSetB(iClientID, "event.enabled", false);
								HookExt::IniSetWS(iClientID, "event.eventid", L"");
								HookExt::IniSetI(iClientID, "event.quantity", 0);
								PrintUserCmdText(iClientID, L"You have been unregistered from the event.");
							}

							Vector vNewTargetPosition;
							vNewTargetPosition.x = jd.vTargetPosition.x + (vPosition.x - vPosition2.x);
							vNewTargetPosition.y = jd.vTargetPosition.y + (vPosition.y - vPosition2.y);
							vNewTargetPosition.z = jd.vTargetPosition.z + (vPosition.z - vPosition2.z);
							pub::Audio::PlaySoundEffect(pPD->iOnlineID, CreateID("dsy_jumpdrive_activate"));
							pub::SpaceObj::DrainShields(pPD->iShipID);
							tunnelTransitTime++;
							SwitchSystem(pPD->iOnlineID, jd.iTargetSystem, vNewTargetPosition, mShipDir2, tunnelTransitTime);
						}
					}
					// Wait until the ship is in the target system before turning off the fuse by
					// holding the timer.
					else if (jd.jump_timer == 1)
					{
						uint iSystem;
						pub::Player::GetSystem(iClientID, iSystem);
						if (iSystem != jd.iTargetSystem)
							jd.jump_timer = 2;
					}
					// Finally turn off the fuse and make sure the ship is damagable
					// (the switch out causes the ship to be invincible
					else if (jd.jump_timer == 0)
					{
						jd.iTargetSystem = 0;
						jd.vTargetPosition.x = 0;
						jd.vTargetPosition.y = 0;
						jd.vTargetPosition.z = 0;
						pub::SpaceObj::SetInvincible(iShip, false, false, 0);
						SetFuse(iClientID, 0);
						StopChargeFuses(iClientID);
					}

					// Proceed to the next ship.
					continue;
				}

				if (jd.charging_on)
				{
					// Use fuel to charge the jump drive's storage capacitors
					jd.charging_on = false;

					if (setCloakingClients.find(iClientID) != setCloakingClients.end())
					{
						PrintUserCmdText(iClientID, L"ERR Ship is cloaked.");
						pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging_failed"));
						SetFuse(iClientID, 0);
						StopChargeFuses(iClientID);
						continue;
					}

					for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
					{
						if (jd.arch->mapFuelToUsage.find(item->iArchID) != jd.arch->mapFuelToUsage.end())
						{
							uint fuel_usage = jd.arch->mapFuelToUsage[item->iArchID];
							if (item->iCount >= fuel_usage)
							{
								pub::Player::RemoveCargo(iClientID, item->sID, fuel_usage);
								jd.curr_charge += jd.arch->charge_rate;
								jd.charging_on = true;
								break;
							}
						}
					}

					// Turn off the charging effect if the charging has failed due to lack of fuel and
					// skip to the next player.
					if (!jd.charging_on)
					{
						PrintUserCmdText(iClientID, L"Jump drive charging failed");
						pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging_failed"));
						SetFuse(iClientID, 0);
						StopChargeFuses(iClientID);
						continue;
					}

					pub::Audio::PlaySoundEffect(iClientID, CreateID("dsy_jumpdrive_charge"));

					if (jd.curr_charge >= jd.arch->can_jump_charge)
					{
						jd.curr_charge = jd.arch->can_jump_charge;
						if (!jd.charging_complete)
						{
							PrintUserCmdText(iClientID, L"Jump drive charging complete, ready to jump");
							pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging_complete"));
							jd.charging_complete = true;
						}
					}
					else
					{
						jd.charging_complete = false;
					}


					uint expected_charge_status = (uint)(jd.curr_charge / jd.arch->can_jump_charge * 10);
					if (jd.charge_status != expected_charge_status)
					{
						jd.charge_status = expected_charge_status;
						PrintUserCmdText(iClientID, L"Jump drive charge %0.0f%%", (jd.curr_charge / jd.arch->can_jump_charge)*100.0f);

						// Find the currently expected charge fuse
						uint charge_fuse_idx = (uint)((jd.curr_charge / jd.arch->can_jump_charge) * (jd.arch->charge_fuse.size() - 1));
						if (charge_fuse_idx >= jd.arch->charge_fuse.size())
							charge_fuse_idx = jd.arch->charge_fuse.size() - 1;

						// If the fuse is not present then activate it.
						uint charge_fuse = jd.arch->charge_fuse[charge_fuse_idx];
						if (find(jd.active_charge_fuse.begin(), jd.active_charge_fuse.end(), charge_fuse)
							== jd.active_charge_fuse.end())
						{
							AddChargeFuse(iClientID, charge_fuse);
						}
					}
				}
				else if (jd.curr_charge > 0.0f)
				{
					// The drive is inactive, discharge the jump capacitors.
					jd.curr_charge -= jd.arch->discharge_rate;
					if (jd.curr_charge < 0.0f)
					{
						jd.curr_charge = 0.0;
					}

					jd.charging_complete = false;
					jd.charge_status = -1;
					StopChargeFuses(iClientID);
					SetFuse(iClientID, 0);
				}
			}
		}

		// If the ship has docked or died remove the client.	
		foreach(lstOldClients, uint, iClientID)
		{
			mapJumpDrives.erase(*iClientID);
		}
	}

	void HyperJump::SendDeathMsg(const wstring &wscMsg, uint iSystem, uint iClientIDVictim, uint iClientIDKiller)
	{
		if (mapActiveBeacons.find(iClientIDVictim) != mapActiveBeacons.end())
		{
			mapActiveBeacons.erase(iClientIDVictim);
		}
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
			if (mapJumpTypeOverride.count(iClientID)) {
				info.iJumpType = mapJumpTypeOverride[iClientID];
			}
			Plugin_Communication(CUSTOM_JUMP, &info);
			return true;
		}
		return false;
	}

	void HyperJump::ClearClientInfo(uint iClientID)
	{
		mapDeferredJumps.erase(iClientID);
		mapJumpDrives.erase(iClientID);
		mapPlayerBeaconMatrix.erase(iClientID);
		mapActiveBeacons.erase(iClientID);
		setCloakingClients.erase(iClientID);
	}

	/** Chase a player. Works across systems but needs improvement of the path selection algorithm */
	void HyperJump::AdminCmd_Chase(CCmds* cmds, const wstring &wscCharname)
	{
		RIGHT_CHECK(RIGHT_CHASEPULL)

		HKPLAYERINFO adminPlyr;
		if (HkGetPlayerInfo(cmds->GetAdminName(), adminPlyr, false) != HKE_OK)
		{
			cmds->Print(L"ERR Not in space\n");
			return;
		}

		HKPLAYERINFO targetPlyr;
		if (HkGetPlayerInfo(wscCharname, targetPlyr, false) != HKE_OK || targetPlyr.iShip == 0)
		{
			cmds->Print(L"ERR Player not found or not in space\n");
			return;
		}

		Vector pos;
		Matrix ornt;
		pub::SpaceObj::GetLocation(targetPlyr.iShip, pos, ornt);
		pos.y += 100;

		cmds->Print(L"Jump to system=%s x=%0.0f y=%0.0f z=%0.0f\n", targetPlyr.wscSystem.c_str(), pos.x, pos.y, pos.z);
		SwitchSystem(adminPlyr.iClientID, targetPlyr.iSystem, pos, ornt);
		return;
	}

	/** Beam admin to a base. Works across systems but needs improvement of the path selection algorithm */
	bool HyperJump::AdminCmd_Beam(CCmds* cmds, const wstring &wscCharname, const wstring &wscTargetBaseName)
	{
		if (!(cmds->rights & RIGHT_BEAMKILL))
		{
			cmds->Print(L"ERR No permission\n");
			return true;;
		}

		HKPLAYERINFO info;
		if (HkGetPlayerInfo(wscCharname, info, false) != HKE_OK)
		{
			cmds->Print(L"ERR Player not found\n");
			return true;
		}

		if (info.iShip == 0)
		{
			cmds->Print(L"ERR Player not in space\n");
			return true;
		}

		// Search for an exact match at the start of the name
		struct Universe::IBase *baseinfo = Universe::GetFirstBase();
		while (baseinfo)
		{
			wstring basename = HkGetWStringFromIDS(baseinfo->iBaseIDS);
			if (ToLower(basename).find(ToLower(wscTargetBaseName)) == 0)
			{
				pub::Player::ForceLand(info.iClientID, baseinfo->iBaseID);
				if (info.iSystem != baseinfo->iSystemID)
				{
					Server.BaseEnter(baseinfo->iBaseID, info.iClientID);
					Server.BaseExit(baseinfo->iBaseID, info.iClientID);
					wstring wscCharFileName;
					HkGetCharFileName(info.wscCharname, wscCharFileName);
					wscCharFileName += L".fl";
					CHARACTER_ID cID;
					strcpy(cID.szCharFilename, wstos(wscCharFileName.substr(0, 14)).c_str());
					Server.CharacterSelect(cID, info.iClientID); \
				}
				return true;
			}
			baseinfo = Universe::GetNextBase();
		}

		// Exact match failed, try a for an partial match
		baseinfo = Universe::GetFirstBase();
		while (baseinfo)
		{
			wstring basename = HkGetWStringFromIDS(baseinfo->iBaseIDS);
			if (ToLower(basename).find(ToLower(wscTargetBaseName)) != -1)
			{
				pub::Player::ForceLand(info.iClientID, baseinfo->iBaseID);
				if (info.iSystem != baseinfo->iSystemID)
				{
					Server.BaseEnter(baseinfo->iBaseID, info.iClientID);
					Server.BaseExit(baseinfo->iBaseID, info.iClientID);
					wstring wscCharFileName;
					HkGetCharFileName(info.wscCharname, wscCharFileName);
					wscCharFileName += L".fl";
					CHARACTER_ID cID;
					strcpy(cID.szCharFilename, wstos(wscCharFileName.substr(0, 14)).c_str());
					Server.CharacterSelect(cID, info.iClientID); \
				}
				return true;
			}
			baseinfo = Universe::GetNextBase();
		}

		// Fall back to default flhook .beam command
		return false;
	}

	/** Pull a player to you. Works across systems but needs improvement of the path selection algorithm */
	void HyperJump::AdminCmd_Pull(CCmds* cmds, const wstring &wscCharname)
	{
		RIGHT_CHECK(RIGHT_CHASEPULL)

		HKPLAYERINFO adminPlyr;
		if (HkGetPlayerInfo(cmds->GetAdminName(), adminPlyr, false) != HKE_OK || adminPlyr.iShip == 0)
		{
			cmds->Print(L"ERR Not in space\n");
			return;
		}

		HKPLAYERINFO targetPlyr;
		if (HkGetPlayerInfo(wscCharname, targetPlyr, false) != HKE_OK)
		{
			cmds->Print(L"ERR Player not found\n");
			return;
		}

		Vector pos;
		Matrix ornt;
		pub::SpaceObj::GetLocation(adminPlyr.iShip, pos, ornt);
		pos.y += 400;

		cmds->Print(L"Jump to system=%s x=%0.0f y=%0.0f z=%0.0f\n", adminPlyr.wscSystem.c_str(), pos.x, pos.y, pos.z);
		SwitchSystem(targetPlyr.iClientID, adminPlyr.iSystem, pos, ornt);
		return;
	}

	/** Move to location */
	void HyperJump::AdminCmd_Move(CCmds* cmds, float x, float y, float z)
	{
		if (cmds->ArgStrToEnd(1).length() == 0)
		{
			cmds->Print(L"ERR Usage: move x y z\n");
			return;
		}

		if (cmds->rights != RIGHT_CHASEPULL)
		{
			cmds->Print(L"ERR No permission\n");
			return;
		}

		HKPLAYERINFO adminPlyr;
		if (HkGetPlayerInfo(cmds->GetAdminName(), adminPlyr, false) != HKE_OK || adminPlyr.iShip == 0)
		{
			cmds->Print(L"ERR Not in space\n");
			return;
		}

		Vector pos;
		Matrix rot;
		pub::SpaceObj::GetLocation(adminPlyr.iShip, pos, rot);
		pos.x = x;
		pos.y = y;
		pos.z = z;
		cmds->Print(L"Moving to %0.0f %0.0f %0.0f\n", pos.x, pos.y, pos.z);
		HkRelocateClient(adminPlyr.iClientID, pos, rot);
		return;
	}

	bool HyperJump::UserCmd_ChargeJumpDrive(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
	{
		// If no ship, report a warning
		IObjInspectImpl *obj = HkGetInspect(iClientID);
		if (!obj)
		{
			PrintUserCmdText(iClientID, L"ERR Jump drive charging failed");
			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging_failed"));
			return true;
		}

		if (!InitJumpDriveInfo(iClientID))
		{
			PrintUserCmdText(iClientID, L"ERR Jump drive not equipped");
			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging_failed"));
			return true;
		}

		JUMPDRIVE &jd = mapJumpDrives[iClientID];

		CShip* cship = (CShip*)HkGetEqObjFromObjRW((IObjRW*)obj);
		if (cship->get_max_power() < jd.arch->power)
		{
			PrintUserCmdText(iClientID, L"Insufficient power to charge jumpdrive, your ship has %f, required: %f", cship->get_max_power(), jd.arch->power);
			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging_failed"));
			return true;
		}

		if (!CanJumpWithCommodities && CheckForCommodities(iClientID)) {
			PrintUserCmdText(iClientID, L"Warning! You cannot jump without clearing your cargo hold!");
		}

		// Toogle the charge state
		jd.charging_on = !jd.charging_on;
		jd.charge_status = -1;

		uint iSystemID;
		pub::Player::GetSystem(iClientID, iSystemID);

		// Start the jump effect
		if (jd.charging_on)
		{
			if (jd.iTargetSystem == 0)
			{
				PrintUserCmdText(iClientID, L"WARNING NO JUMP COORDINATES");
				pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_blind_jump_warning"));
			}
			////////////////////////// System limit restriction ///////////////////////////////////////
			else if (JumpSystemListEnabled && !IsSystemJumpable(iSystemID, jd.iTargetSystem, jd.arch->jump_range))
			{
				const Universe::ISystem *iSysList = Universe::get_system(jd.iTargetSystem);
				wstring wscSysNameList = HkGetWStringFromIDS(iSysList->strid_name);
				PrintUserCmdText(iClientID, L"ERROR: Gravitational rift detected. Cannot jump to %s from this system.", wscSysNameList.c_str());
				PrintUserCmdText(iClientID, L"Jump drive disabled. Use /jumpsys for the list of available systems.");
				jd.charging_complete = false;
				jd.curr_charge = 0.0;
				jd.charging_on = false;
				StopChargeFuses(iClientID);
				return true;
			}
			////////////////////////// End of System limit restriction ///////////////////////////////////////

			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging"));
			PrintUserCmdText(iClientID, L"Jump drive charging");
			// Print out a message within the iLocalChatRange when a player engages a JD.
			wstring wscMsg = L"%time WARNING: A hyperspace breach is being opened by %player";
			wscMsg = ReplaceStr(wscMsg, L"%time", GetTimeString(set_bLocalTime));
			wscMsg = ReplaceStr(wscMsg, L"%player", (const wchar_t*)Players.GetActiveCharacterName(iClientID));
			PrintLocalUserCmdText(iClientID, wscMsg, set_iLocalChatRange);
		}
		// Cancel jump effect if it is running
		else
		{
			PrintUserCmdText(iClientID, L"Jump drive disabled");
			StopChargeFuses(iClientID);
			SetFuse(iClientID, 0);
		}
		return true;
	}

	bool HyperJump::UserCmd_ActivateJumpDrive(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
	{
		// If no ship, report a warning
		IObjInspectImpl *obj = HkGetInspect(iClientID);
		if (!obj)
		{
			PrintUserCmdText(iClientID, L"Jump drive charging failed");
			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging_failed"));
			return true;
		}

		// If no jumpdrive, report a warning.
		if (!InitJumpDriveInfo(iClientID))
		{
			PrintUserCmdText(iClientID, L"ERR Jump drive not equipped");
			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging_failed"));
			return true;
		}

		JUMPDRIVE &jd = mapJumpDrives[iClientID];

		// If insufficient charging, report a warning
		if (!jd.charging_complete)
		{
			PrintUserCmdText(iClientID, L"Jump drive not ready");
			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_not_ready"));
			return true;
		}
		uint clientSystem;
		pub::Player::GetSystem(iClientID, clientSystem);
		if (mapAvailableJumpSystems.count(clientSystem) == 0) {
			PrintUserCmdText(iClientID, L"ERR Jumping from this system is not possible");
			return true;
		}

		if (jd.iTargetSystem == 0)
		{

			PrintUserCmdText(iClientID, L"WARNING NO JUMP COORDINATES");
			PrintUserCmdText(iClientID, L"BLIND JUMP ACTIVATED");

			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_blind_jump_warning"));

			if (BlindJumpOverrideSystem != 0)
			{
				if (!mapSystemJumps.count(BlindJumpOverrideSystem)) {
					PrintUserCmdText(iClientID, L"ERR Server Config Issue: blind jump target unavailable, contact staff");
					return true;
				}
				const auto& overridenDestinationList = mapSystemJumps[BlindJumpOverrideSystem];
				const auto& overrideDestination = overridenDestinationList.at(rand() % overridenDestinationList.size());
				mapJumpDrives[iClientID].iTargetSystem = BlindJumpOverrideSystem;
				mapJumpDrives[iClientID].vTargetPosition = overrideDestination.pos;
				mapJumpDrives[iClientID].matTargetOrient = overrideDestination.ornt;
			}
			else if (!JumpSystemListEnabled)
			{
				vector<string> systems;
				for (struct Universe::ISystem *sysinfo = Universe::GetFirstSystem(); sysinfo; sysinfo = Universe::GetNextSystem())
				{
					systems.push_back(sysinfo->nickname);
				}
				// Pick a random system and position
				jd.iTargetSystem = CreateID(systems[rand() % systems.size()].c_str());
			}
			else
			{
				if (mapAvailableJumpSystems.count(clientSystem)) {
					auto& systemListForSyst = mapAvailableJumpSystems[clientSystem];
					auto& systemListAtRandRange = systemListForSyst[(rand() % jd.arch->jump_range)+1];
					uint selectedSystem = systemListAtRandRange.at(rand() % systemListAtRandRange.size());
					if (mapSystemJumps.count(selectedSystem) == 0) {
						PrintUserCmdText(iClientID, L"ERR Issue performing a jump to system hash %u, contact staff", selectedSystem);
						return true;
					}
					auto& coordList = mapSystemJumps[selectedSystem];
					auto& coords = coordList.at(rand() % coordList.size());
					jd.iTargetSystem = selectedSystem;
					jd.vTargetPosition = coords.pos;
					jd.matTargetOrient = coords.ornt;
				}

				// If can't jump - notify.
				if (jd.iTargetSystem == 0)
				{
					PrintUserCmdText(iClientID, L"Jump drive malfunction. Cannot jump from the system.");
					pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging_failed"));
					return true;
				}
			}
		}
		//notify group members
		if (jd.arch->group_jump_range > 0)
		{
			list<GROUP_MEMBER> lstGrpMembers;
			HkGetGroupMembers((const wchar_t*)Players.GetActiveCharacterName(iClientID), lstGrpMembers);

			Vector pos;
			Matrix orn;
			uint clientShipID;
			pub::Player::GetShip(iClientID, clientShipID);
			pub::SpaceObj::GetLocation(clientShipID, pos, orn);
			for (const auto& member : lstGrpMembers)
			{
				if (member.iClientID == iClientID)
					continue;
				uint memberSystemID;
				pub::Player::GetSystem(member.iClientID, memberSystemID);
				if (clientSystem != memberSystemID)
					continue;
				uint memberShipID;
				pub::Player::GetShip(member.iClientID, memberShipID);
				if (memberShipID == 0)
					continue;
				Vector memberPos;
				Matrix memberOrn;
				pub::SpaceObj::GetLocation(memberShipID, memberPos, memberOrn);
				
				float distance = HkDistance3D(pos, memberPos);

				if (HkDistance3D(pos, memberPos) <= jd.arch->group_jump_range)
				{
					PrintUserCmdText(member.iClientID, L"Info: Group Jump initiated, you are in range.");
				}
				else
				{
					PrintUserCmdText(member.iClientID, L"Warning: Group Jump initiated, but you are out of range by %um!", static_cast<uint>(distance - jd.arch->group_jump_range));
				}
			}
		}

		// Start the jump timer.
		jd.jump_timer = 8;

		return true;
	}

	time_t filetime_to_timet(const FILETIME& ft) {
		ULARGE_INTEGER ull;
		ull.LowPart = ft.dwLowDateTime;
		ull.HighPart = ft.dwHighDateTime;
		return ull.QuadPart / 10000000ULL - 11644473600ULL;
	}

	// Move the ship's starting position randomly if it has been logged out in space.
	void HyperJump::PlayerLaunch(unsigned int iShip, unsigned int iClientID)
	{
		static const uint MAX_DRIFT = 50000;

		// Find the time this ship was last online.
		wstring wscTimeStamp = L"";
		if (HkFLIniGet((const wchar_t*)Players.GetActiveCharacterName(iClientID), L"tstamp", wscTimeStamp) != HKE_OK)
			return;

		FILETIME ft;
		ft.dwHighDateTime = strtoul(GetParam(wstos(wscTimeStamp), ',', 0).c_str(), 0, 10);
		ft.dwLowDateTime = strtoul(GetParam(wstos(wscTimeStamp), ',', 1).c_str(), 0, 10);
		time_t lastTime = filetime_to_timet(ft);

		// Get the current time; note FL stores the FILETIME in local time not UTC.
		SYSTEMTIME st;
		GetLocalTime(&st);
		SystemTimeToFileTime(&st, &ft);
		time_t currTime = filetime_to_timet(ft);

		// Calculate the expected drift.
		float drift = (float)(currTime - lastTime);
		wstring wscRights;
		HkGetAdmin((const wchar_t*)Players.GetActiveCharacterName(iClientID), wscRights);
		if (drift > MAX_DRIFT)
			drift = MAX_DRIFT;

		drift *= ((2.0f * rand() / (float)RAND_MAX) - 1.0f);

		// Adjust the ship's position.
		Vector pos = { Players[iClientID].vPosition.x, Players[iClientID].vPosition.y, Players[iClientID].vPosition.z };
		pos.x += drift / 10;
		pos.y += drift;
		pos.z += drift / 10;
		pub::Player::SetInitialPos(iClientID, pos);
	}

	void HyperJump::MissileTorpHit(uint iClientID, DamageList *dmg)
	{

		//TEMPORARY: Allow JDs to be disrupted with CDs
		if (mapJumpDrives.find(iClientID) != mapJumpDrives.end())
		{
			if (mapJumpDrives[iClientID].charging_on && mapJumpDrives[iClientID].arch->cd_disrupts_charge)
			{
				if (dmg->get_cause() == 6)
				{
					mapJumpDrives[iClientID].charging_on = false;
					mapJumpDrives[iClientID].curr_charge = 0;
					PrintUserCmdText(iClientID, L"Jump drive disrupted. Charging failed.");
					pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging_failed"));
					SetFuse(iClientID, 0);
					StopChargeFuses(iClientID);
				}
			}
		}
	}

	bool HyperJump::UserCmd_DeployBeacon(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
	{

		HKPLAYERINFO p;
		if (HkGetPlayerInfo((const wchar_t*)Players.GetActiveCharacterName(iClientID), p, false) != HKE_OK || p.iShip == 0)
		{
			PrintUserCmdText(iClientID, L"ERR Not in space");
			return true;
		}

		if (!CheckForBeacon(iClientID))
		{
			PrintUserCmdText(iClientID, L"ERR No hyperspace matrix device found.");
			return true;
		}

		for (map<uint, BEACONTIMER>::iterator i = mapActiveBeacons.begin(); i != mapActiveBeacons.end(); ++i)
		{
			if (i->first == iClientID)
			{
				if (i->second.cooldown != 0)
				{
					PrintUserCmdText(iClientID, L"Hyperspace generator currently recharging. %d seconds left.", i->second.cooldown);
					return true;
				}
			}
		}

		for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
		{
			if (item->iArchID == mapPlayerBeaconMatrix[iClientID]->item)
			{
				if (item->get_count() < mapPlayerBeaconMatrix[iClientID]->itemcount)
				{
					PrintUserCmdText(iClientID, L"ERR Not enough batteries to power matrix.");
					return true;
				}

				pub::Player::RemoveCargo(iClientID, item->sID, mapPlayerBeaconMatrix[iClientID]->itemcount);

				const wchar_t* playerName = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
				// Print out a message within the iLocalChatRange when a player engages a JD.
				wstring wscMsg = L"%time WARNING: A hyperspace beacon has been activated by %player";
				wscMsg = ReplaceStr(wscMsg, L"%time", GetTimeString(set_bLocalTime));
				wscMsg = ReplaceStr(wscMsg, L"%player", playerName);
				PrintLocalUserCmdText(iClientID, wscMsg, set_iLocalChatRange);
				// End of local message
                
				list<GROUP_MEMBER> lstGrpMembers;
				HkGetGroupMembers((const wchar_t*)Players.GetActiveCharacterName(iClientID), lstGrpMembers);
                
                for(GROUP_MEMBER& member : lstGrpMembers){
                    if(member.iClientID != iClientID)
                    {
                        PrintUserCmdText(member.iClientID, L"Hyperspace beacon has been activated by %ls, it will remain active for %u seconds", playerName, BeaconTime);
                    }
                }

				IObjInspectImpl *obj = HkGetInspect(iClientID);
				if (obj)
				{
					HkLightFuse((IObjRW*)obj, BeaconFuse, 0, static_cast<float>(BeaconTime), 0);
				}

				BEACONTIMER bc;
				bc.cooldown = BeaconCooldown;
				bc.timeleft = BeaconTime;
				bc.decayed = false;

				mapActiveBeacons[iClientID] = bc;

				return true;
			}
		}

		PrintUserCmdText(iClientID, L"No hyperspace beacon found.");
		return true;
	}

	bool HyperJump::UserCmd_JumpBeacon(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
	{
		// Indicate an error if the command does not appear to be formatted correctly 
		// and stop processing but tell FLHook that we processed the command.
		if (wscParam.empty())
		{
			PrintUserCmdText(iClientID, L"ERR Invalid parameters");
			PrintUserCmdText(iClientID, usage);
			return true;
		}

		// If no ship, report a warning
		IObjInspectImpl *obj = HkGetInspect(iClientID);
		if (!obj)
		{
			PrintUserCmdText(iClientID, L"Jump drive charging failed");
			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging_failed"));
			return true;
		}

		// If no jumpdrive, report a warning.
		if (!InitJumpDriveInfo(iClientID))
		{
			PrintUserCmdText(iClientID, L"ERR Jump drive not equipped");
			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging_failed"));
			return true;
		}

		JUMPDRIVE &jd = mapJumpDrives[iClientID];

		wstring wscCharname = GetParam(wscParam, L' ', 0);
		uint iTargetClientID = HkGetClientIdFromCharname(wscCharname);
		if (iTargetClientID == UINT_MAX) {
			uint targetClientID = ToUInt(wscCharname);
			if (targetClientID && HkIsValidClientID(targetClientID))
			{
				iTargetClientID = targetClientID;
			}
			else
			{
				PrintUserCmdText(iClientID, L"ERR Player not online");
				return true;
			}
		}
		obj = HkGetInspect(iTargetClientID);
		if (!obj)
		{
			PrintUserCmdText(iClientID, L"ERR Ship not found");
			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_not_ready"));
			return true;
		}

		if (mapActiveBeacons[iTargetClientID].decayed == true)
		{
			PrintUserCmdText(iClientID, L"ERR No active beacon found.");
			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_not_ready"));
			return true;
		}

		// If insufficient charging, report a warning
		if (!jd.charging_complete)
		{
			PrintUserCmdText(iClientID, L"Jump drive not ready");
			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_not_ready"));
			return true;
		}

		uint iShipTarget;
		pub::Player::GetShip(iTargetClientID, iShipTarget);

		uint iSystemID;
		pub::Player::GetSystem(iClientID, iSystemID);

		uint iTargetSystem;
		Vector pos;
		Matrix rot;
		pub::SpaceObj::GetLocation(iShipTarget, pos, rot);
		pub::Player::GetSystem(iTargetClientID, iTargetSystem);

		////////////////////////// System limit restriction ///////////////////////////////////////
		if (JumpSystemListEnabled && !IsSystemJumpable(iSystemID, iTargetSystem, (jd.arch->jump_range + mapPlayerBeaconMatrix[iTargetClientID]->range)))
		{
			const Universe::ISystem *iSysList = Universe::get_system(iTargetSystem);
			wstring wscSysNameList = HkGetWStringFromIDS(iSysList->strid_name);
			PrintUserCmdText(iClientID, L"ERROR: Gravitational rift detected. Cannot jump to %s from this system.", wscSysNameList.c_str());
			PrintUserCmdText(iClientID, L"Jump drive disabled. Use /jumpsys for the list of available systems.");
			jd.charging_complete = false;
			jd.curr_charge = 0.0;
			jd.charging_on = false;
			StopChargeFuses(iClientID);
			return true;
			
		}
		////////////////////////// End of System limit restriction ///////////////////////////////////////

		jd.iTargetSystem = iTargetSystem;
		jd.vTargetPosition = pos;
		jd.matTargetOrient = rot;

		const struct Universe::ISystem *sysinfo = Universe::get_system(jd.iTargetSystem);
		PrintUserCmdText(iClientID, L"OK Beacon coordinates verified: %s %0.0f.%0.0f.%0.0f",
			HkGetWStringFromIDS(sysinfo->strid_name).c_str(),
			*(float*)&jd.vTargetPosition.x,
			*(float*)&jd.vTargetPosition.y,
			*(float*)&jd.vTargetPosition.z);

		int wiggle_factor = (int)mapPlayerBeaconMatrix[iTargetClientID]->accuracy;
		jd.vTargetPosition.x += ((rand() * 10) % wiggle_factor*2) - wiggle_factor;
		jd.vTargetPosition.y += ((rand() * 10) % wiggle_factor*2) - wiggle_factor;
		jd.vTargetPosition.z += ((rand() * 10) % wiggle_factor*2) - wiggle_factor;

		// Start the jump timer.
		jd.jump_timer = 8;
		return true;
	}

	void HyperJump::ForceJump(CUSTOM_JUMP_CALLOUT_STRUCT jumpData) {
		mapJumpTypeOverride[jumpData.iClientID] = jumpData.jumpType;
		SwitchSystem(jumpData.iClientID, jumpData.iSystemID, jumpData.pos, jumpData.ori);
	}
}