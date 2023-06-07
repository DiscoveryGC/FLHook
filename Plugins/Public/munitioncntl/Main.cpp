// MunitionControl Plugin - Handle tracking/alert notifications for missile projectiles
// and mine behaviour on expiration
// By Aingar
//
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Includes
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <unordered_set>
#include <unordered_map>
#include <FLHook.h>
#include <plugin.h>

map<string, uint> factions;
/// A return code to indicate to FLHook if we want the hook processing to continue.
PLUGIN_RETURNCODE returncode;

unordered_set<uint> setNoTrackingAlertProjectiles;
unordered_set<uint> setNoFuseOnExpiryMines;

unordered_map<uint, uint> mapTrackingByShiptypeBlacklistBitmap;

bool enableMineExpiryFuse = false;

enum TRACKING_STATE {
	TRACK_ALERT,
	TRACK_NOALERT,
	NOTRACK_NOALERT
};

void LoadSettings();

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	srand((uint)time(0));
	// If we're being loaded from the command line while FLHook is running then
	// set_scCfgFile will not be empty so load the settings as FLHook only
	// calls load settings on FLHook startup and .rehash.
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		if (set_scCfgFile.length() > 0)
			LoadSettings();
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
	}
	return true;
}

/// Hook will call this function after calling a plugin function to see if we the
/// processing to continue
EXPORT PLUGIN_RETURNCODE Get_PluginReturnCode()
{
	return returncode;
}

bool bPluginEnabled = true;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Loading Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	INI_Reader ini;

	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\missilecntl.cfg";
	if (ini.open(scPluginCfgFile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("General"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("EnableMineExpiryFuse"))
					{
						enableMineExpiryFuse = ini.get_value_bool(0);
					}
				}
			}
			else if (ini.is_header("NoTrackingAlertProjectile"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("MissileArch"))
					{
						setNoTrackingAlertProjectiles.insert(CreateID(ini.get_value_string(0)));
					}
				}
			}
			else if (ini.is_header("SetNoFuseOnExpiryMines"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("MineArch"))
					{
						setNoFuseOnExpiryMines.insert(CreateID(ini.get_value_string(0)));
					}
				}
			}
			else if (ini.is_header("TrackingBlacklistByShipType"))
			{
				uint missileArch = 0;
				uint blacklistedShipTypesBitmap = 0;
				while (ini.read_value())
				{
					if (ini.is_value("MissileArch"))
					{
						missileArch = CreateID(ini.get_value_string(0));
					}
					else if (ini.is_value("DontTrackShipType"))
					{
						string typeStr = ToLower(ini.get_value_string(0));
						if (typeStr == "fighter")
							blacklistedShipTypesBitmap |= OBJ_FIGHTER;
						else if (typeStr == "freighter")
							blacklistedShipTypesBitmap |= OBJ_FREIGHTER;
						else if (typeStr == "transport")
							blacklistedShipTypesBitmap |= OBJ_TRANSPORT;
						else if (typeStr == "gunboat")
							blacklistedShipTypesBitmap |= OBJ_GUNBOAT;
						else if (typeStr == "cruiser")
							blacklistedShipTypesBitmap |= OBJ_CRUISER;
						else if (typeStr == "capital")
							blacklistedShipTypesBitmap |= OBJ_CAPITAL;
						else
							ConPrint(L"MissileCntl: Error reading config for Blacklisted munitions, value %ls not recognized\n", stows(typeStr).c_str());
					}
				}
				mapTrackingByShiptypeBlacklistBitmap[missileArch] = blacklistedShipTypesBitmap;
			}
		}
		ini.close();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __stdcall CreateGuided(uint iClientID, FLPACKET_CREATEGUIDED& createGuidedPacket)
{
	returncode = DEFAULT_RETURNCODE;

	uint ownerType;
	pub::SpaceObj::GetType(createGuidedPacket.iOwner, ownerType);
	if (!(ownerType & (OBJ_FIGHTER | OBJ_FREIGHTER | OBJ_TRANSPORT | OBJ_GUNBOAT | OBJ_CRUISER | OBJ_CAPITAL))) //GetTarget throws an exception for non-ship entities.
		return;
	uint targetId;
	pub::SpaceObj::GetTarget(createGuidedPacket.iOwner, targetId);

	TRACKING_STATE tracking = TRACK_ALERT;

	if (!targetId) // prevent missiles from tracking cloaked ships, and missiles sticking targeting to last selected target
	{
		tracking = NOTRACK_NOALERT;
	}
	else if (setNoTrackingAlertProjectiles.count(createGuidedPacket.iMunitionId)) // for 'dumbified' seeker missiles, disable alert, used for flaks and snub dumbfires
	{
		tracking = TRACK_NOALERT;
	}
	else if (mapTrackingByShiptypeBlacklistBitmap.count(createGuidedPacket.iMunitionId)) // disable tracking for selected ship types
	{
		uint targetType;
		pub::SpaceObj::GetType(createGuidedPacket.iTargetId, targetType);
		const auto& blacklistedShipTypeTargets = mapTrackingByShiptypeBlacklistBitmap.at(createGuidedPacket.iMunitionId);
		if(blacklistedShipTypeTargets & targetType)
		{
			tracking = NOTRACK_NOALERT;
		}
	}

	switch (tracking)
	{
		case NOTRACK_NOALERT:
		{
			const auto& projectile = reinterpret_cast<CGuided*>(CObject::Find(createGuidedPacket.iProjectileId, CObject::CGUIDED_OBJECT));
			projectile->set_target(nullptr); //disable tracking, switch fallthrough to also disable alert
		}
		case TRACK_NOALERT:
		{
			createGuidedPacket.iTargetId = 0; // prevents the 'incoming missile' warning client-side
		}
	}
}

bool __stdcall DestroyObject(uint iClientID, FLPACKET_DESTROYOBJECT& pDestroy)
{
	//This hook looks for packets informing players of a mine that is about to expire: Type = OBJ_MINE and DestroyType = VANISH
	//If caught, we supress that packet from being sent and trigger the explosion via Destroy method with FUSE destroy type.
	returncode = DEFAULT_RETURNCODE;

	if (!enableMineExpiryFuse || pDestroy.iDestroyType == FUSE)
	{
		return true;
	}

	uint type;
	pub::SpaceObj::GetType(pDestroy.iSpaceID, type);
	if (type == OBJ_MINE)
	{
		if (!setNoFuseOnExpiryMines.empty())
		{
			uint mineArch;
			pub::SpaceObj::GetSolarArchetypeID(pDestroy.iSpaceID, mineArch);
			if (setNoFuseOnExpiryMines.count(mineArch))
			{
				return true;
			}
		}
		pub::SpaceObj::Destroy(pDestroy.iSpaceID, FUSE);
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions to hook
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "Missile Controller";
	p_PI->sShortName = "missilecntl";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CreateGuided, PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_CREATEGUIDED, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&DestroyObject, PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_DESTROYOBJECT, 0));

	return p_PI;
}
