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



map<uint, POBSOUNDS> soundhistory;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Structures and shit yo
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Siege::SiegeGunDeploy(uint client, const wstring& args)
{
	// Abort processing if this is not a "heavy lifter"
	uint shiparch;
	pub::Player::GetShipID(client, shiparch);
	if (set_construction_shiparch != 0 && shiparch != set_construction_shiparch)
	{
		PrintUserCmdText(client, L"ERR Need deployment ship");
		return;
	}

	uint ship;
	pub::Player::GetShip(client, ship);
	if (!ship)
	{
		PrintUserCmdText(client, L"ERR Not in space");
		return;
	}

	// If the ship is moving, abort the processing.
	Vector dir1;
	Vector dir2;
	pub::SpaceObj::GetMotion(ship, dir1, dir2);
	if (dir1.x > 5 || dir1.y > 5 || dir1.z > 5)
	{
		PrintUserCmdText(client, L"ERR Ship is moving");
		return;
	}

	int min = 100;
	int max = 5000;
	int randomsiegeint = min + (rand() % (int)(max - min + 1));

	string randomname = "Siege Cannon AX-" + randomsiegeint;

	// Check for conflicting base name
	if (GetPlayerBase(CreateID(PlayerBase::CreateBaseNickname(randomname).c_str())))
	{
		PrintUserCmdText(client, L"ERR Deployment error, please reiterate.");
		return;
	}

	// Check that the ship has the requires commodities.
	int hold_size;
	list<CARGO_INFO> cargo;
	HkEnumCargo((const wchar_t*)Players.GetActiveCharacterName(client), cargo, hold_size);
	for (map<uint, uint>::iterator i = construction_items.begin(); i != construction_items.end(); ++i)
	{
		bool material_available = false;
		uint good = i->first;
		uint quantity = i->second;
		for (list<CARGO_INFO>::iterator ci = cargo.begin(); ci != cargo.end(); ++ci)
		{
			if (ci->iArchID == good && ci->iCount >= (int)quantity)
			{
				material_available = true;
				pub::Player::RemoveCargo(client, ci->iID, quantity);
			}
		}
		if (material_available == false)
		{
			PrintUserCmdText(client, L"ERR Construction failed due to insufficient raw material.");
			for (i = construction_items.begin(); i != construction_items.end(); ++i)
			{
				const GoodInfo* gi = GoodList::find_by_id(i->first);
				if (gi)
				{
					PrintUserCmdText(client, L"|  %ux %s", i->second, HkGetWStringFromIDS(gi->iIDSName).c_str());
				}
			}
			return;
		}
	}

	wstring charname = (const wchar_t*)Players.GetActiveCharacterName(client);
	AddLog("NOTICE: Base created %s by %s (%s)",
		randomname.c_str(),
		wstos(charname).c_str(),
		wstos(HkGetAccountID(HkGetAccountByCharname(charname))).c_str());

	wstring password = L"hastesucks";
	wstring basename = stows(randomname);

	PlayerBase* newbase = new PlayerBase(client, password, basename);
	player_bases[newbase->base] = newbase;
	newbase->basetype = "siegegun";
	newbase->basesolar = "depot";
	newbase->baseloadout = "depot";
	newbase->defense_mode = 1;

	newbase->invulnerable = mapArchs[newbase->basetype].invulnerable;
	newbase->logic = mapArchs[newbase->basetype].logic;

	newbase->Spawn();
	newbase->Save();

	PrintUserCmdText(client, L"OK: Siege Cannon deployed");
	PrintUserCmdText(client, L"Default administration password is %s", password.c_str());
}

int Siege::CalculateHealthPercentage(uint basehash, int health, int maxhealth)
{
	int result = 100 * (health / maxhealth);



	return result;
}

void Siege::SiegeAudioCalc(uint basehash, uint iSystemID, Vector pos, int level)
{
	// For all players in system...
	struct PlayerData* pPD = 0;
	while (pPD = Players.traverse_active(pPD))
	{
		// Get the this player's current system and location in the system.
		uint iClientID = HkGetClientIdFromPD(pPD);

		uint iClientSystem = 0;
		pub::Player::GetSystem(iClientID, iClientSystem);
		if (iSystemID != iClientSystem)
			continue;

		uint iShip;
		pub::Player::GetShip(iClientID, iShip);

		Vector vShipLoc;
		Matrix mShipDir;
		pub::SpaceObj::GetLocation(iShip, vShipLoc, mShipDir);

		// Is player within (15K) of the sending char.
		float fDistance = HkDistance3D(vShipLoc, pos);
		if (fDistance > 15000)
			continue;

		SiegeAudioNotification(iClientID, level);
	}
	return;
}

int Siege::GetRandomSound(int min, int max)
{
	srand((unsigned)time(0));
	int result = min + (rand() % (int)(max - min + 1));
	return result;
}

void Siege::SiegeAudioNotification(uint iClientID, int level)
{
	//0 = base destruction
	if (level == 0)
	{
		int sound = GetRandomSound(1, 2);
		if (sound == 1)
		{
			pub::Audio::PlaySoundEffect(iClientID, pbsounds.destruction1);
		}
		else if (sound == 2)
		{
			pub::Audio::PlaySoundEffect(iClientID, pbsounds.destruction2);
		}
		else
		{
			HkMsgU(L"Debug: no possible sound for level 0");
		}
		return;
	}


	return;
}
