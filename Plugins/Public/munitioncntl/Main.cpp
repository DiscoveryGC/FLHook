// MunitionControl Plugin - Handle tracking/alert notifications for missile projectiles
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

unordered_map<uint, uint> mapTrackingByObjTypeBlacklistBitmap;
uint lastProcessedProjectile = 0;

constexpr uint shipObjType = (OBJ_FIGHTER | OBJ_FREIGHTER | OBJ_TRANSPORT | OBJ_GUNBOAT | OBJ_CRUISER | OBJ_CAPITAL);

enum TRACKING_STATE {
	TRACK_ALERT,
	TRACK_NOALERT,
	NOTRACK_NOALERT
};

void LoadSettings();

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	// If we're being loaded from the command line while FLHook is running then
	// set_scCfgFile will not be empty so load the settings as FLHook only
	// calls load settings on FLHook startup and .rehash.
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		if (set_scCfgFile.length() > 0)
		{
			LoadSettings();
		}
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Loading Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	INI_Reader ini;

	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + R"(\flhook_plugins\munitioncntl.cfg)";
	if (!ini.open(scPluginCfgFile.c_str(), false))
	{
		return;
	}
	while (ini.read_header())
	{
		if (ini.is_header("NoTrackingAlert"))
		{
			while (ini.read_value())
			{
				if (ini.is_value("MissileArch"))
				{
					setNoTrackingAlertProjectiles.insert(CreateID(ini.get_value_string(0)));
				}
			}
		}
		else if (ini.is_header("TrackingBlacklist"))
		{
			uint missileArch = 0;
			uint blacklistedTrackingTypesBitmap = 0;
			while (ini.read_value())
			{
				if (ini.is_value("MissileArch"))
				{
					missileArch = CreateID(ini.get_value_string(0));
				}
				else if (ini.is_value("Type"))
				{
					string typeStr = ToLower(ini.get_value_string(0));
					if (typeStr.find("fighter") != string::npos)
						blacklistedTrackingTypesBitmap |= OBJ_FIGHTER;
					if (typeStr.find("freighter") != string::npos)
						blacklistedTrackingTypesBitmap |= OBJ_FREIGHTER;
					if (typeStr.find("transport") != string::npos)
						blacklistedTrackingTypesBitmap |= OBJ_TRANSPORT;
					if (typeStr.find("gunboat") != string::npos)
						blacklistedTrackingTypesBitmap |= OBJ_GUNBOAT;
					if (typeStr.find("cruiser") != string::npos)
						blacklistedTrackingTypesBitmap |= OBJ_CRUISER;
					if (typeStr.find("capital") != string::npos)
						blacklistedTrackingTypesBitmap |= OBJ_CAPITAL;
					if (typeStr.find("guided") != string::npos)
						blacklistedTrackingTypesBitmap |= OBJ_GUIDED;
					if (typeStr.find("mine") != string::npos)
						blacklistedTrackingTypesBitmap |= OBJ_MINE;
				}
			}
			if (missileArch && blacklistedTrackingTypesBitmap)
			{
				mapTrackingByObjTypeBlacklistBitmap[missileArch] = blacklistedTrackingTypesBitmap;
			}
			else
			{
				ConPrint(L"MunitionCntl: Error! Incomplete [TrackingBlacklist] definition in config files!\n");
			}
		}
	}
	ini.close();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProcessGuided(FLPACKET_CREATEGUIDED& createGuidedPacket)
{
	uint ownerType;
	pub::SpaceObj::GetType(createGuidedPacket.iOwner, ownerType);
	if (!(ownerType & shipObjType)) //GetTarget throws an exception for non-ship entities.
	{
		return;
	}
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
	else if (mapTrackingByObjTypeBlacklistBitmap.count(createGuidedPacket.iMunitionId)) // disable tracking for selected ship types
	{
		uint targetType;
		pub::SpaceObj::GetType(createGuidedPacket.iTargetId, targetType);
		const auto& blacklistedShipTypeTargets = mapTrackingByObjTypeBlacklistBitmap.at(createGuidedPacket.iMunitionId);
		if (blacklistedShipTypeTargets & targetType)
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

void __stdcall CreateGuided(uint& iClientID, FLPACKET_CREATEGUIDED& createGuidedPacket)
{
	returncode = DEFAULT_RETURNCODE;

	//Packet hooks are executed once for every player in range, but we only need to process the missile packet once, since it's passed by reference.
	if (lastProcessedProjectile == createGuidedPacket.iProjectileId)
	{
		return;
	}

	lastProcessedProjectile = createGuidedPacket.iProjectileId;
	ProcessGuided(createGuidedPacket); 
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions to hook
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "Munition Controller";
	p_PI->sShortName = "munitioncntl";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;

	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CreateGuided, PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_CREATEGUIDED, 0));

	return p_PI;
}
