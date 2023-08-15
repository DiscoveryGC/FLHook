// Player Control plugin for FLHookPlugin
// Feb 2010 by Cannon
//
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

// This code from somewhere, credit to motah, wodka and mc_horst.

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

#include <PluginUtilities.h>
#include "Main.h"

namespace AntiJumpDisconnect
{
	static map<uint, bool> isClientInJumpTunnelMap;

	void AntiJumpDisconnect::ClearClientInfo(unsigned int iClientID)
	{
		isClientInJumpTunnelMap[iClientID] = false;
	}

	void AntiJumpDisconnect::DisConnect(unsigned int iClientID, enum  EFLConnection state)
	{
		if (isClientInJumpTunnelMap[iClientID])
		{
			uint iShip;
			pub::Player::GetShip(iClientID, iShip);
			pub::SpaceObj::SetInvincible(iShip, false, false, 0);
			IObjInspectImpl *obj = HkGetInspect(iClientID);
			if (obj)
			{
				HkLightFuse((IObjRW*)obj, CreateID("death_comm"), 0.0f, 0.0f, 0.0f);
			}
			HkTempBan(iClientID, 5);
		}
	}

	void AntiJumpDisconnect::CharacterInfoReq(unsigned int iClientID, bool p2)
	{
		if (isClientInJumpTunnelMap[iClientID])
		{
			uint iShip;
			pub::Player::GetShip(iClientID, iShip);
			pub::SpaceObj::SetInvincible(iShip, false, false, 0);
			IObjInspectImpl *obj = HkGetInspect(iClientID);
			if (obj)
			{
				HkLightFuse((IObjRW*)obj, CreateID("death_comm"), 0.0f, 0.0f, 0.0f);
			}
			HkTempBan(iClientID, 5);
		}
	}

	void AntiJumpDisconnect::JumpInComplete(unsigned int iSystem, unsigned int iShip, unsigned int iClientID)
	{
		isClientInJumpTunnelMap[iClientID] = false;
	}

	void AntiJumpDisconnect::SystemSwitchOut(uint iClientID)
	{
		isClientInJumpTunnelMap[iClientID] = true;
	}

	bool AntiJumpDisconnect::IsInWarp(uint iClientID)
	{
		return isClientInJumpTunnelMap[iClientID];
	}
}