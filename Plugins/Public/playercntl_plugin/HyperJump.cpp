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
	static map<wstring, uint> mapRestrictedShips;

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

	static map<uint, vector<uint>> mapJumpSystems;

	static int JumpWhiteListEnabled = 0;
	static int JumpSystemListEnabled = 0;
	static int SurveyPlaneLimit = 10000;
	static uint BeaconCommodity = 0;
	static int BeaconTime = 120;
	static int BeaconCooldown = 300;
	static uint BeaconFuse = 0;

	struct DEFERREDJUMPS
	{
		uint system;
		Vector pos;
		Matrix ornt;
	};
	static map<uint, DEFERREDJUMPS> mapDeferredJumps;

	struct JUMPDRIVE_ARCH
	{
		uint nickname;
		float can_jump_charge;
		float charge_rate;
		float discharge_rate;
		vector<uint> charge_fuse;
		uint jump_fuse;
		map<uint, uint> mapFuelToUsage;
		float power;
		float field_range;
		boolean cd_disrupts_charge;
	};
	static map<uint, JUMPDRIVE_ARCH> mapJumpDriveArch;

	struct SURVEY_ARCH
	{
		uint nickname;
		float survey_complete_charge;
		float charge_rate;
		map<uint, uint> mapFuelToUsage;
		float power;
		float coord_accuracy;
	};
	static map<uint, SURVEY_ARCH> mapSurveyArch;

	struct SURVEY
	{
		SURVEY_ARCH arch;
		float curr_charge;
		bool charging_on;
		Vector pos_start;
	};
	static map<uint, SURVEY> mapSurvey;

	struct JUMPDRIVE
	{
		JUMPDRIVE_ARCH arch;

		bool charging_on;
		float curr_charge;
		uint active_fuse;
		list<uint> active_charge_fuse;
		bool charging_complete;
		uint charge_status;

		int jump_timer;
		uint iTargetSystem;
		Vector vTargetPosition;

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
		uint item;
		int itemcount;
	};

	//There is only one kind of Matrix right now, but this might change later on
	static map<uint, BEACONMATRIX> mapBeaconMatrix;
	//map the existing Matrix
	static map<uint, BEACONMATRIX> mapPlayerBeaconMatrix;

#define HCOORD_SIZE 28
	struct HYPERSPACE_COORDS
	{
		WORD parity;
		WORD seed;
		DWORD system;
		float x;
		float y;
		float z;
		DWORD time;
		float accuracy;
	};

	static string set_scEncryptKey = "secretcode";

	static wstring FormatCoords(char* ibuf)
	{
		wstring sbuf;
		wchar_t buf[100];
		for (int i = 0; i < HCOORD_SIZE; i++)
		{
			if (i != 0 && (i % 4) == 0) sbuf += L"-";
			_snwprintf(buf, sizeof(buf), L"%02X", (byte)ibuf[i]);
			sbuf += buf;
		}
		return sbuf;
	}

	static void EncryptDecrypt(char *ibuf, char *obuf)
	{
		obuf[0] = ibuf[0];
		obuf[1] = ibuf[1];
		for (uint i = 2, p = ibuf[0] % set_scEncryptKey.length(); i < HCOORD_SIZE; i++, p++)
		{
			if (p > set_scEncryptKey.length())
				p = 0;
			obuf[i] = ibuf[i] ^ set_scEncryptKey[p];
		}
	}

	void SwitchSystem(uint iClientID, uint system, Vector pos, Matrix ornt)
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

	void HyperJump::LoadSettings(const string &scPluginCfgFile)
	{
		// Patch Archetype::GetEquipment & Archetype::GetShip to suppress annoying warnings flserver-errors.log
		unsigned char patch1[] = { 0x90, 0x90 };
		WriteProcMem((char*)0x62F327E, &patch1, 2);
		WriteProcMem((char*)0x62F944E, &patch1, 2);
		WriteProcMem((char*)0x62F123E, &patch1, 2);

		set_scEncryptKey = Players.GetServerSig();

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
						else if (ini.is_value("SurveyPlaneLimit"))
						{
							SurveyPlaneLimit = ini.get_value_int(0);
							ConPrint(L"HYPERJUMP NOTICE: Survey Plane Limit is %u \n", SurveyPlaneLimit);
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
					}
				}
				else if (ini.is_header("shiprestrictions"))
				{
					while (ini.read_value())
					{
						if (ini.is_value("restrict"))
						{
							string nickname = ini.get_value_string(0);
							uint iArchID = CreateID(nickname.c_str());
							wstring wstnickname = stows(nickname);
							mapRestrictedShips[wstnickname] = iArchID;
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
						else if (ini.is_value("cd_disrupts_charge"))
						{
							jd.cd_disrupts_charge = ini.get_value_bool(0);
						}
					}
					mapJumpDriveArch[jd.nickname] = jd;
				}
				else if (ini.is_header("survey"))
				{
					SURVEY_ARCH hs;
					while (ini.read_value())
					{
						if (ini.is_value("nickname"))
						{
							hs.nickname = CreateValidID(ini.get_value_string(0));
						}
						else if (ini.is_value("survey_complete_charge"))
						{
							hs.survey_complete_charge = ini.get_value_float(0);
						}
						else if (ini.is_value("charge_rate"))
						{
							hs.charge_rate = ini.get_value_float(0);
						}
						else if (ini.is_value("fuel"))
						{
							uint fuel = CreateValidID(ini.get_value_string(0));
							int rate = ini.get_value_int(1);
							hs.mapFuelToUsage[fuel] = rate;
						}
						else if (ini.is_value("power"))
						{
							hs.power = ini.get_value_float(0);
						}
						else if (ini.is_value("coord_accuracy"))
						{
							hs.coord_accuracy = ini.get_value_float(0);
						}
					}
					mapSurveyArch[hs.nickname] = hs;
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
			while (ini.read_header())
			{
				if (ini.is_header("system"))
				{
					uint nickname;
					vector<uint> visibleSystemsList;
					while (ini.read_value())
					{
						if (ini.is_value("nickname"))
						{
							nickname = CreateID(ini.get_value_string(0));
						}
						else if (ini.is_value("jump"))
						{
							visibleSystemsList.push_back(CreateID(ini.get_value_string(0)));
						}
					}
					mapJumpSystems[nickname] = visibleSystemsList;
				}
			}
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

	void SetFuse(uint iClientID, uint fuse)
	{
		JUMPDRIVE &jd = mapJumpDrives[iClientID];
		if (jd.active_fuse)
		{
			IObjInspectImpl *obj = HkGetInspect(iClientID);
			if (obj)
			{
				HkUnLightFuse((IObjRW*)obj, jd.active_fuse, 0.0f);
			}
			jd.active_fuse = 0;
		}

		if (fuse)
		{
			IObjInspectImpl *obj = HkGetInspect(iClientID);
			if (obj)
			{
				jd.active_fuse = fuse;
				HkLightFuse((IObjRW*)obj, jd.active_fuse, 0.0f, 0.0f, 0.0f);
			}
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

	bool HyperJump::UserCmd_ListJumpableSystems(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
	{
		uint iSystemID;
		pub::Player::GetSystem(iClientID, iSystemID);
		const Universe::ISystem *iSys = Universe::get_system(iSystemID);
		wstring wscSysName = HkGetWStringFromIDS(iSys->strid_name);

		PrintUserCmdText(iClientID, L"Space kitteh knows you are in %s !", wscSysName.c_str());

		if (JumpWhiteListEnabled == 1)
		{
			for (map<uint, vector<uint>>::iterator iter = mapJumpSystems.begin(); iter != mapJumpSystems.end(); iter++)
			{
				vector<uint> &systemList = iter->second;
				if (iter->first == iSystemID)
				{
					PrintUserCmdText(iClientID, L"The script has found %s !", wscSysName.c_str());
					PrintUserCmdText(iClientID, L"You are allowed to jump to:");

					for (uint &allowed_sys : systemList)
					{
						const Universe::ISystem *iSysList = Universe::get_system(allowed_sys);
						wstring wscSysNameList = HkGetWStringFromIDS(iSysList->strid_name);
						PrintUserCmdText(iClientID, L"|     %s", wscSysNameList.c_str());
					}
				}
			}
		}
		else
		{
			PrintUserCmdText(iClientID, L"Jump System Whitelisting is not enabled.");
		}
		return true;
	}

	bool IsSystemJumpable(uint systemFrom, uint systemTo)
	{
		auto allowedSystemIter = mapJumpSystems.find(systemFrom);

		if (allowedSystemIter == mapJumpSystems.end())
			return false;

		vector<uint> &allowedSystems = allowedSystemIter->second;
		return find(allowedSystems.begin(), allowedSystems.end(), systemTo) != allowedSystems.end();
	}

	/** List ships restricted from jumping */
	void HyperJump::AdminCmd_ListRestrictedShips(CCmds* cmds)
	{
		if (cmds->rights != RIGHT_SUPERADMIN)
		{
			cmds->Print(L"ERR No permission\n");
			return;
		}

		cmds->Print(L"Ships restricted from jumping: %d", mapRestrictedShips.size());
		for (map<wstring, uint>::iterator i = mapRestrictedShips.begin();
			i != mapRestrictedShips.end(); ++i)
		{
			cmds->Print(L"|     %s\n", i->first.c_str());
		}
		cmds->Print(L"OK\n");

		return;
	}

	void HyperJump::AdminCmd_MakeCoord(CCmds* cmds)
	{
		if (cmds->rights != RIGHT_SUPERADMIN)
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

		Vector v;
		Matrix m;
		pub::SpaceObj::GetLocation(adminPlyr.iShip, v, m);

		// Fill out the coordinate structure
		HYPERSPACE_COORDS coords;
		char* ibuf = (char*)&coords;
		memset(&coords, 0, sizeof(coords));
		coords.seed = rand();
		coords.system = Players[adminPlyr.iClientID].iSystemID;
		coords.x = v.x;
		coords.y = v.y;
		coords.z = v.z;
		coords.time = (uint)time(0) + (35 * 24 * 3600);
		coords.accuracy = 1;

		// Calculate a simple parity check
		WORD parity = 0;
		for (int i = 2; i < HCOORD_SIZE; i++)
			parity += ibuf[i];
		coords.parity = parity;

		// Encrypt it
		char obuf[HCOORD_SIZE];
		EncryptDecrypt(ibuf, obuf);
		PrintUserCmdText(adminPlyr.iClientID, L"Hyperspace survey complete");
		//PrintUserCmdText(iClientID, L"Raw: %s", FormatCoords(ibuf).c_str());		
		PrintUserCmdText(adminPlyr.iClientID, L"Coords: %s", FormatCoords(obuf).c_str());

		return;
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

		// Handle survey charging
		for (map<uint, SURVEY>::iterator iter = mapSurvey.begin(); iter != mapSurvey.end(); iter++)
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
				SURVEY &sm = iter->second;
				if (sm.charging_on)
				{
					//Remember starting location of survey
					Matrix ornt;
					if (sm.curr_charge <= sm.arch.charge_rate)
						pub::SpaceObj::GetLocation(iShip, sm.pos_start, ornt);

					// Use fuel to charge the jump drive's storage capacitors
					sm.charging_on = false;

					for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
					{
						if (sm.arch.mapFuelToUsage.find(item->iArchID) != sm.arch.mapFuelToUsage.end())
						{
							uint fuel_usage = sm.arch.mapFuelToUsage[item->iArchID];
							if (item->iCount >= fuel_usage)
							{
								pub::Player::RemoveCargo(iClientID, item->sID, fuel_usage);
								sm.curr_charge += sm.arch.charge_rate;
								sm.charging_on = true;
								break;
							}
						}
					}

					Vector pos_curr;
					pub::SpaceObj::GetLocation(iShip, pos_curr, ornt);
					
					//Allow hyperspace survey charge to 500m from starting position
					if (HkDistance3D(pos_curr,sm.pos_start)> 500)
					{
						sm.charging_on = false;
					}

					// Turn off the charging effect if the charging has failed due to lack of fuel and
					// skip to the next player.
					if (!sm.charging_on)
					{
						sm.curr_charge = 0;
						PrintUserCmdText(iClientID, L"Survey failed");
						continue;
					}

					if (sm.curr_charge >= sm.arch.survey_complete_charge)
					{
						// We're done.
						Vector v;
						Matrix m;
						pub::SpaceObj::GetLocation(iShip, v, m);

						// Fill out the coordinate structure
						HYPERSPACE_COORDS coords;
						char* ibuf = (char*)&coords;
						memset(&coords, 0, sizeof(coords));
						coords.seed = rand();
						coords.system = Players[iClientID].iSystemID;
						coords.x = v.x;
						coords.y = v.y;
						coords.z = v.z;
						coords.time = (uint)time(0) + (35 * 24 * 3600);
						coords.accuracy = sm.arch.coord_accuracy;

						// Calculate a simple parity check
						WORD parity = 0;
						for (int i = 2; i < HCOORD_SIZE; i++)
							parity += ibuf[i];
						coords.parity = parity;

						// Encrypt it
						char obuf[HCOORD_SIZE];
						EncryptDecrypt(ibuf, obuf);
						PrintUserCmdText(iClientID, L"Hyperspace survey complete");
						PrintUserCmdText(iClientID, L"Coords: %s", FormatCoords(obuf).c_str());

						lstOldClients.push_back(iClientID);
					}
					else if (time(0) % 10 == 0)
					{
						PrintUserCmdText(iClientID, L"Survey %0.0f%% complete", (sm.curr_charge / sm.arch.survey_complete_charge)*100.0f);
					}
				}
			}
		}

		foreach(lstOldClients, uint, iClientID)
		{
			mapSurvey.erase(*iClientID);
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
						SetFuse(iClientID, jd.arch.jump_fuse);
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
						Archetype::Ship *ship = Archetype::GetShip(Players[iClientID].iShipArchetype);
						wstring playershiparch = stows(ship->szName);
						if (mapRestrictedShips.find(playershiparch) != mapRestrictedShips.end() && JumpWhiteListEnabled == 1)
						{
							PrintUserCmdText(iClientID, L"ERR Ship is not allowed to jump.");
						}
						else
						{
							SwitchSystem(iClientID, jd.iTargetSystem, jd.vTargetPosition, mShipDir);
						}

						// Find all ships within the jump field including the one with the jump engine.
						if (jd.arch.field_range > 0)
						{
							struct PlayerData *pPD = 0;
							while (pPD = Players.traverse_active(pPD))
							{
								uint iSystemID2;
								pub::SpaceObj::GetSystem(pPD->iShipID, iSystemID2);

								Vector vPosition2;
								Matrix mShipDir2;
								pub::SpaceObj::GetLocation(pPD->iShipID, vPosition2, mShipDir2);

								if (pPD->iOnlineID != iClientID
									&& iSystemID2 == iSystemID
									&& HkDistance3D(vPosition, vPosition2) <= jd.arch.field_range)
								{
									// Restrict some ships from jumping, this is for the jumpers
									Archetype::Ship *ship = Archetype::GetShip(pPD->iShipArchetype);
									wstring playershiparch = stows(ship->szName);
									if (mapRestrictedShips.find(playershiparch) != mapRestrictedShips.end() && JumpWhiteListEnabled == 1)
									{
										PrintUserCmdText(pPD->iOnlineID, L"ERR Ship is not allowed to jump.");
									}
									else
									{
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
										SwitchSystem(pPD->iOnlineID, jd.iTargetSystem, vNewTargetPosition, mShipDir2);
									}
								}
							}
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
						if (jd.arch.mapFuelToUsage.find(item->iArchID) != jd.arch.mapFuelToUsage.end())
						{
							uint fuel_usage = jd.arch.mapFuelToUsage[item->iArchID];
							if (item->iCount >= fuel_usage)
							{
								pub::Player::RemoveCargo(iClientID, item->sID, fuel_usage);
								jd.curr_charge += jd.arch.charge_rate;
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

					if (jd.curr_charge >= jd.arch.can_jump_charge)
					{
						jd.curr_charge = jd.arch.can_jump_charge;
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


					uint expected_charge_status = (uint)(jd.curr_charge / jd.arch.can_jump_charge * 10);
					if (jd.charge_status != expected_charge_status)
					{
						jd.charge_status = expected_charge_status;
						PrintUserCmdText(iClientID, L"Jump drive charge %0.0f%%", (jd.curr_charge / jd.arch.can_jump_charge)*100.0f);

						// Find the currently expected charge fuse
						uint charge_fuse_idx = (uint)((jd.curr_charge / jd.arch.can_jump_charge) * (jd.arch.charge_fuse.size() - 1));
						if (charge_fuse_idx >= jd.arch.charge_fuse.size())
							charge_fuse_idx = jd.arch.charge_fuse.size() - 1;

						// If the fuse is not present then activate it.
						uint charge_fuse = jd.arch.charge_fuse[charge_fuse_idx];
						if (find(jd.active_charge_fuse.begin(), jd.active_charge_fuse.end(), charge_fuse)
							== jd.active_charge_fuse.end())
						{
							AddChargeFuse(iClientID, charge_fuse);
						}
					}
				}
				else
				{
					// The drive is inactive, discharge the jump capacitors.
					jd.curr_charge -= jd.arch.discharge_rate;
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
			Plugin_Communication(CUSTOM_JUMP, &info);
			return true;
		}
		return false;
	}

	void HyperJump::ClearClientInfo(uint iClientID)
	{
		mapDeferredJumps.erase(iClientID);
		mapJumpDrives.erase(iClientID);
		mapSurvey.erase(iClientID);
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

		if (cmds->rights != RIGHT_SUPERADMIN)
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

	bool InitJumpDriveInfo(uint iClientID)
	{
		// Initialise the drive parameters for this ship
		if (mapJumpDrives.find(iClientID) == mapJumpDrives.end())
		{
			mapJumpDrives[iClientID].arch.nickname = 0;
			mapJumpDrives[iClientID].arch.can_jump_charge = 0;
			mapJumpDrives[iClientID].arch.charge_fuse.clear();
			mapJumpDrives[iClientID].arch.charge_rate = 0;
			mapJumpDrives[iClientID].arch.discharge_rate = 0;
			mapJumpDrives[iClientID].arch.jump_fuse = 0;
			mapJumpDrives[iClientID].arch.mapFuelToUsage.clear();
			mapJumpDrives[iClientID].arch.cd_disrupts_charge = true;

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
						mapJumpDrives[iClientID].arch = mapJumpDriveArch[item->iArchID];
						return true;
					}
				}
			}

			return false;
		}

		return true;
	}

	bool CheckForMatrix(uint iClientID)
	{
		for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
		{
			if (mapBeaconMatrix.find(item->iArchID) != mapBeaconMatrix.end())
			{
				mapPlayerBeaconMatrix[iClientID] = mapBeaconMatrix[item->iArchID];
				return true;
			}
		}

		return true;
	}

	bool InitSurveyInfo(uint iClientID)
	{
		// Initialise the drive parameters for this ship
		if (mapSurvey.find(iClientID) == mapSurvey.end())
		{
			mapSurvey[iClientID].arch.nickname = 0;
			mapSurvey[iClientID].arch.survey_complete_charge = 0;
			mapSurvey[iClientID].arch.charge_rate = 0;
			mapSurvey[iClientID].arch.coord_accuracy = 0;
			mapSurvey[iClientID].arch.power = 0;
			mapSurvey[iClientID].arch.mapFuelToUsage.clear();

			mapSurvey[iClientID].charging_on = false;
			mapSurvey[iClientID].curr_charge = 0;

			// Check that the player has a jump drive and initialise the infomation
			// about it - otherwise return false.
			for (list<EquipDesc>::iterator item = Players[iClientID].equipDescList.equip.begin(); item != Players[iClientID].equipDescList.equip.end(); item++)
			{
				if (mapSurveyArch.find(item->iArchID) != mapSurveyArch.end())
				{
					mapSurvey[iClientID].arch = mapSurveyArch[item->iArchID];
					return true;
				}
			}

			return false;
		}

		return true;
	}

	bool HyperJump::UserCmd_Survey(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
	{
		// If no ship, report a warning
		IObjInspectImpl *obj = HkGetInspect(iClientID);
		if (!obj)
		{
			PrintUserCmdText(iClientID, L"Survey module not available");
			return true;
		}

		if (!InitSurveyInfo(iClientID))
		{
			PrintUserCmdText(iClientID, L"Survey module not available");
			return true;
		}

		SURVEY &sm = mapSurvey[iClientID];

		CShip* cship = (CShip*)HkGetEqObjFromObjRW((IObjRW*)obj);
		if (cship->get_max_power() < sm.arch.power)
		{
			PrintUserCmdText(iClientID, L"Insufficient power to start survey module");
			return true;
		}

		HKPLAYERINFO p;
		if (HkGetPlayerInfo((const wchar_t*)Players.GetActiveCharacterName(iClientID), p, false) != HKE_OK || p.iShip == 0)
		{
			PrintUserCmdText(iClientID, L"ERR Not in space");
			return true;
		}

		Vector pos;
		Matrix rot;
		pub::SpaceObj::GetLocation(p.iShip, pos, rot);

		////////////////////////// Plane survey restriction ///////////////////////////////////////
		////////////////////////// x up / x down from plane ///////////////////////////////////////
		if (pos.y >= SurveyPlaneLimit || pos.y <= -SurveyPlaneLimit)
		{
			PrintUserCmdText(iClientID, L"ERR Distance from stellar plane too far, unable to compensate for galactic drift. Move within %u of stellar plane.", SurveyPlaneLimit);
			return true;
		}
		// Toogle the charge state
		sm.charging_on = !sm.charging_on;
		if (sm.charging_on)
		{
			PrintUserCmdText(iClientID, L"Survey started");
			sm.curr_charge = 0;
			return true;
		}
		else
		{
			PrintUserCmdText(iClientID, L"Survey aborted");
			sm.curr_charge = 0;
			return true;
		}
	}

	bool HyperJump::UserCmd_SetCoords(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
	{
		if (!InitJumpDriveInfo(iClientID))
		{
			PrintUserCmdText(iClientID, L"Jump drive not available");
			return true;
		}


		JUMPDRIVE &jd = mapJumpDrives[iClientID];

		if (jd.jump_timer)
		{
			PrintUserCmdText(iClientID, L"Unable to change coordinates during jump.");
			return true;
		}

		jd.iTargetSystem = 0;

		string sbuf = wstos(ReplaceStr(GetParam(wscParam, L' ', 0), L"-", L""));
		if (sbuf.size() != 56)
		{
			PrintUserCmdText(iClientID, L"ERR Invalid coordinates, format error");
			return true;
		}

		char ibuf[HCOORD_SIZE];
		for (uint i = 0, p = 0; i < HCOORD_SIZE && (p + 1) < sbuf.size(); i++, p += 2)
		{
			char buf[3];
			buf[0] = sbuf[p];
			buf[1] = sbuf[p + 1];
			buf[2] = 0;
			ibuf[i] = (char)strtoul(buf, 0, 16);
		}

		HYPERSPACE_COORDS coords;
		char* obuf = (char*)&coords;
		memset(&coords, 0, sizeof(coords));

		EncryptDecrypt(ibuf, obuf);

		// Calculate a simple parity check
		WORD parity = 0;
		for (int i = 2; i < HCOORD_SIZE; i++)
			parity += obuf[i];

		if (coords.parity != parity)
		{
			PrintUserCmdText(iClientID, L"ERR Invalid coordinates, parity error");
			return true;
		}

		////////////////////////// System limit restriction ///////////////////////////////////////
		if (JumpSystemListEnabled)
		{
			bool AllowedToWhiteListJump = IsSystemJumpable(Players[iClientID].iSystemID, coords.system);
			if (!AllowedToWhiteListJump)
			{
				const Universe::ISystem *iSysList = Universe::get_system(coords.system);
				wstring wscSysNameList = HkGetWStringFromIDS(iSysList->strid_name);
				PrintUserCmdText(iClientID, L"ERROR: Gravitational rift detected. Cannot jump to %s from this system.", wscSysNameList.c_str());
				return true;
			}
		}
		////////////////////////// End of System limit restriction ///////////////////////////////////////

		if (coords.time < time(0))
		{
			PrintUserCmdText(iClientID, L"Warning old coordinates detected. Jump not recommended");
			coords.accuracy *= rand() % 6 + 1;
		}

		jd.iTargetSystem = coords.system;
		jd.vTargetPosition.x = coords.x;
		jd.vTargetPosition.y = coords.y;
		jd.vTargetPosition.z = coords.z;

		const struct Universe::ISystem *sysinfo = Universe::get_system(jd.iTargetSystem);
		PrintUserCmdText(iClientID, L"OK Coordinates verified: %s %0.0f.%0.0f.%0.0f",
			HkGetWStringFromIDS(sysinfo->strid_name).c_str(),
			*(float*)&jd.vTargetPosition.x,
			*(float*)&jd.vTargetPosition.y,
			*(float*)&jd.vTargetPosition.z);

		int wiggle_factor = (int)coords.accuracy;
		jd.vTargetPosition.x += ((rand() * 10) % wiggle_factor) - (wiggle_factor / 2);
		jd.vTargetPosition.y += ((rand() * 10) % wiggle_factor) - (wiggle_factor / 2);
		jd.vTargetPosition.z += ((rand() * 10) % wiggle_factor) - (wiggle_factor / 2);

		return true;
	}

	bool HyperJump::UserCmd_ChargeJumpDrive(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
	{
		// If no ship, report a warning
		IObjInspectImpl *obj = HkGetInspect(iClientID);
		if (!obj)
		{
			PrintUserCmdText(iClientID, L"Jump drive charging failed");
			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging_failed"));
			return true;
		}

		if (!InitJumpDriveInfo(iClientID))
		{
			PrintUserCmdText(iClientID, L"Jump drive not available");
			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging_failed"));
			return true;
		}

		JUMPDRIVE &jd = mapJumpDrives[iClientID];

		CShip* cship = (CShip*)HkGetEqObjFromObjRW((IObjRW*)obj);
		if (cship->get_max_power() < jd.arch.power)
		{
			PrintUserCmdText(iClientID, L"Insufficient power to charge jumpdrive");
			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging_failed"));
			return true;
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
			else if (JumpSystemListEnabled)
			{
				bool AllowedToWhiteListJump = IsSystemJumpable(iSystemID, jd.iTargetSystem);
				if (!AllowedToWhiteListJump)
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
			PrintUserCmdText(iClientID, L"Jump drive not ready");
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

		if (jd.iTargetSystem == 0)
		{
			PrintUserCmdText(iClientID, L"WARNING NO JUMP COORDINATES");
			PrintUserCmdText(iClientID, L"BLIND JUMP ACTIVATED");

			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_blind_jump_warning"));

			if (!JumpSystemListEnabled)
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
				auto systemListIter = mapJumpSystems.find(Players[iClientID].iSystemID);
				if (systemListIter != mapJumpSystems.end())
				{
					auto &systemList = systemListIter->second;
					if (!systemList.empty())
					{
						jd.iTargetSystem = systemList.at(rand() % systemList.size());
					}
				}

				// If can't jump - notify.
				if (jd.iTargetSystem == 0)
				{
					PrintUserCmdText(iClientID, L"Jump drive malfunction. Cannot jump from the system.");
					pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging_failed"));
					return true;
				}
			}

			jd.vTargetPosition.x = ((rand() * 10) % 400000) - 200000.0f;
			jd.vTargetPosition.y = ((rand() * 10) % 400000) - 200000.0f;
			jd.vTargetPosition.z = ((rand() * 10) % 400000) - 200000.0f;
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
		if (mapSurvey.find(iClientID) != mapSurvey.find(iClientID))
		{
			if (dmg->get_cause() == 6 || dmg->get_cause() == 0x15)
			{
				mapSurvey[iClientID].curr_charge = 0;
				mapSurvey[iClientID].charging_on = false;
				PrintUserCmdText(iClientID, L"Hyperspace survey disrupted, restart required");
			}
		}

		//TEMPORARY: Allow JDs to be disrupted with CDs
		if (mapJumpDrives.find(iClientID) != mapJumpDrives.end())
		{
			if (mapJumpDrives[iClientID].charging_on && mapJumpDrives[iClientID].arch.cd_disrupts_charge)
			{
				if (dmg->get_cause() == 6 || dmg->get_cause() == 0x15)
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

		if (mapPlayerBeaconMatrix.find(iClientID) == mapPlayerBeaconMatrix.end())
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
			if (item->iArchID == mapPlayerBeaconMatrix[iClientID].item)
			{
				if (item->get_count() < mapPlayerBeaconMatrix[iClientID].itemcount)
				{
					PrintUserCmdText(iClientID, L"ERR Not enough batteries to power matrix.");
					return true;
				}

				pub::Player::RemoveCargo(iClientID, item->sID, mapPlayerBeaconMatrix[iClientID].itemcount);

				// Print out a message within the iLocalChatRange when a player engages a JD.
				wstring wscMsg = L"%time WARNING: A hyperspace beacon has been activated by %player";
				wscMsg = ReplaceStr(wscMsg, L"%time", GetTimeString(set_bLocalTime));
				wscMsg = ReplaceStr(wscMsg, L"%player", (const wchar_t*)Players.GetActiveCharacterName(iClientID));
				PrintLocalUserCmdText(iClientID, wscMsg, set_iLocalChatRange);
				// End of local message

				IObjInspectImpl *obj = HkGetInspect(iClientID);
				if (obj)
				{
					HkLightFuse((IObjRW*)obj, BeaconFuse, 0, BeaconTime, 0);
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
		if (wscParam.size() == 0)
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
			PrintUserCmdText(iClientID, L"Jump drive not ready");
			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_charging_failed"));
			return true;
		}

		JUMPDRIVE &jd = mapJumpDrives[iClientID];

		wstring wscCharname = GetParam(wscParam, L' ', 0);
		uint iClientIDTarget = HkGetClientIdFromCharname(wscCharname);

		//Check if the client and the ship exist. Extra caution? Probably, but this command won't be run often so it's acceptable.
		if (iClientIDTarget)
		{
			IObjInspectImpl *obj = HkGetInspect(iClientIDTarget);
			if (!obj)
			{
				PrintUserCmdText(iClientID, L"ERR Ship not found");
				pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_not_ready"));
				return true;
			}

			if (mapActiveBeacons.find(iClientIDTarget) == mapActiveBeacons.end())
			{
				PrintUserCmdText(iClientID, L"ERR No active beacon found.");
				pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_not_ready"));
				return true;
			}

			if (mapActiveBeacons[iClientIDTarget].decayed == true)
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
			pub::Player::GetShip(iClientIDTarget, iShipTarget);

			uint iSystemID;
			pub::Player::GetSystem(iClientID, iSystemID);

			uint iTargetSystem;
			Vector pos;
			Matrix rot;
			pub::SpaceObj::GetLocation(iShipTarget, pos, rot);
			pub::Player::GetSystem(iClientIDTarget, iTargetSystem);


			////////////////////////// System limit restriction ///////////////////////////////////////
			if (JumpSystemListEnabled == 1)
			{
				bool AllowedToWhiteListJump = IsSystemJumpable(iSystemID, iTargetSystem);
				if (!AllowedToWhiteListJump)
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
			}
			////////////////////////// End of System limit restriction ///////////////////////////////////////

			jd.iTargetSystem = iTargetSystem;
			jd.vTargetPosition.x = pos.x;
			jd.vTargetPosition.y = pos.y;
			jd.vTargetPosition.z = pos.z;

			const struct Universe::ISystem *sysinfo = Universe::get_system(jd.iTargetSystem);
			PrintUserCmdText(iClientID, L"OK Beacon coordinates verified: %s %0.0f.%0.0f.%0.0f",
				HkGetWStringFromIDS(sysinfo->strid_name).c_str(),
				*(float*)&jd.vTargetPosition.x,
				*(float*)&jd.vTargetPosition.y,
				*(float*)&jd.vTargetPosition.z);

			int wiggle_factor = (int)mapPlayerBeaconMatrix[iClientIDTarget].accuracy;
			jd.vTargetPosition.x += ((rand() * 10) % wiggle_factor) - (wiggle_factor / 2);
			jd.vTargetPosition.y += ((rand() * 10) % wiggle_factor) - (wiggle_factor / 2);
			jd.vTargetPosition.z += ((rand() * 10) % wiggle_factor) - (wiggle_factor / 2);

			// Start the jump timer.
			jd.jump_timer = 8;
			return true;
		}
		else
		{
			PrintUserCmdText(iClientID, L"ERR Name not found");
			pub::Player::SendNNMessage(iClientID, pub::GetNicknameId("nnv_jumpdrive_not_ready"));
			return true;
		}

		return true;
	}

	void HyperJump::Disrupt(uint iTargetID, uint iClientID)
	{
		if (mapJumpDrives.find(iTargetID) != mapJumpDrives.end())
		{
			if (mapJumpDrives[iTargetID].charging_on == true)
			{
				mapJumpDrives[iTargetID].charging_on = false;
				PrintUserCmdText(iTargetID, L"Jump drive disrupted. Charging failed.");
				PrintUserCmdText(iClientID, L"Jump drive disruption successful");
				pub::Player::SendNNMessage(iTargetID, pub::GetNicknameId("nnv_jumpdrive_charging_failed"));
				SetFuse(iTargetID, 0);
				StopChargeFuses(iTargetID);
			}
		}
	}

}
