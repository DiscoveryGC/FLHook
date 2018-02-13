#include "Main.h"
#include <hookext_exports.h>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

void DeleteBase(PlayerBase *base)
{
	// If there are players online and in the base then force them to launch to space
	struct PlayerData *pd = 0;
	while (pd = Players.traverse_active(pd))
	{
		uint client = pd->iOnlineID;
		if (HkIsInCharSelectMenu(client))
			continue;

		// If this player is in space, set the reputations.
		if (!pd->iShipID)
			continue;

		// Get state if player is in player base and  reset the commodity list
		// and send a dummy entry if there are no commodities in the market
		if (clients[client].player_base == base->base)
		{
			// Force the ship to launch to space as the base has been destroyed
			DeleteDockState(client);
			SendResetMarketOverride(client);
			ForceLaunch(client);
		}
	}

	// Remove the base.
	//_unlink(base->path.c_str());

	//Edit by Alley: Don't remove the base, instead move it to an archive folder
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);
	// Create base save  dir if it doesn't exist
	string basesvdir = string(datapath) + "\\Accts\\MultiPlayer\\player_bases\\destroyed\\";
	CreateDirectoryA(basesvdir.c_str(), 0);

	string timestamp = boost::posix_time::to_iso_string(boost::posix_time::second_clock::local_time());

	char namehash[16];
	sprintf(namehash, "%08x", base->base);

	string fullpath = basesvdir + "base_" + namehash + "." + timestamp + ".ini";
	if (!MoveFile(base->path.c_str(), fullpath.c_str())) {
		AddLog(
			"ERROR: Base destruction MoveFile FAILED! Error code: %s",
			boost::lexical_cast<std::string>(GetLastError()).c_str()
		);
	}

	player_bases.erase(base->base);
	delete base;
}

void LoadDockState(uint client)
{
	clients[client].player_base = HookExt::IniGetI(client, "base.player_base");
	clients[client].last_player_base = HookExt::IniGetI(client, "base.last_player_base");
}

void SaveDockState(uint client)
{
	HookExt::IniSetI(client, "base.player_base", clients[client].player_base);
	HookExt::IniSetI(client, "base.last_player_base", clients[client].last_player_base);
}

void DeleteDockState(uint client)
{
	HookExt::IniSetI(client, "base.player_base", 0);
	HookExt::IniSetI(client, "base.last_player_base", 0);
}