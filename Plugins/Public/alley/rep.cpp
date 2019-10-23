// AlleyPlugin for FLHookPlugin
// April 2015 by Alley
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
#include "PlayerRestrictions.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Structures and shit yo
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void REP::LoadSettings()
{
	// The path to the configuration file.
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\alley_rep.cfg";

	INI_Reader ini;
	if (ini.open(scPluginCfgFile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("reputations"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("rep"))
					{
						uint solarnick = CreateID(ini.get_value_string(0));
						const char* newrep = ini.get_value_string(1);

						ConPrint(L"DEBUG: Rep of %s is %s \n", stows(ini.get_value_string(0)).c_str(), stows(ini.get_value_string(1)).c_str());

						uint obj_rep_group;
						pub::Reputation::GetReputationGroup(obj_rep_group, newrep);
						pub::Reputation::SetAffiliation(solarnick, obj_rep_group);
					}
				}
			}
		}
		ini.close();
	}
}
