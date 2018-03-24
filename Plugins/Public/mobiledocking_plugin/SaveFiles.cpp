#include "Main.h"

// A function used to save a docked player to the filesystem
void SaveDockInfoCarried(uint clientID, const CLIENT_DATA& client)
{
	// The path to the data file.
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	char tpath[1024];
	sprintf(tpath, R"(%s\Accts\MultiPlayer\docking_module\docked_%08x.ini)", datapath, clientID);
	string path = tpath;

	FILE *file = fopen(path.c_str(), "w");
	if(file)
	{
		fprintf(file, "[Ship]\n");
		fprintf(file, "clientid=%u\n", clientID);
		fprintf(file, "dockedwith=%ls\n", client.wscDockedWithCharname.c_str());
		fprintf(file, "lastdockedbase=%u\n", client.iLastBaseID);
	}

	fclose(file);
}

// A function used to save a carrier player to the filesystem.
void SaveDockInfoCarrier(uint clientID, const CLIENT_DATA& client)
{
	// The path to the data file.
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	char tpath[1024];
	sprintf(tpath, R"(%s\Accts\MultiPlayer\docking_module\carrier_%08x.ini)", datapath, clientID);
	string path = tpath;

	FILE *file = fopen(path.c_str(), "w");
	if(file)
	{
		ConPrint(L"Doing this nooow\n");
		fprintf(file, "[Carrier]\n");
		fprintf(file, "clientid=%u\n", clientID);
		
		// Save each docked ship name
		for (map<wstring,wstring>::const_iterator it = client.mapDockedShips.begin(); it != client.mapDockedShips.end(); ++it)
		{
			ConPrint(L"For something\n");
			fprintf(file, "dockedchar=%ls, %ls\n", it->first.c_str(), it->second.c_str());
		}

		fprintf(file, "availablemodules=%u\n", client.iDockingModules);
		fprintf(file, "lastdocked=%u\n", client.iLastBaseID);
	}
	fclose(file);
}

void LoadShip(uint clientID)
{
	// The path to the data file.
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	char tpath[1024];
	sprintf(tpath, R"(%s\Accts\MultiPlayer\docking_module\carrier_%08x.ini)", datapath, clientID);
	string path = tpath;

	INI_Reader ini;

	// Attempt to load the ship as a carrier
	if (ini.open(path.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("Carrier"))
			{
				uint shipClientId;
				CLIENT_DATA carrierInfo;

				while (ini.read_value())
				{
					if (ini.is_value("clientid"))
					{
						shipClientId = ini.get_value_int(0);
					}
					else if (ini.is_value("dockedchar"))
					{
						carrierInfo.mapDockedShips[stows(ini.get_value_string(0))] = stows(ini.get_value_string(1));
					}
					else if (ini.is_value("lastdocked"))
					{
						carrierInfo.iLastBaseID = ini.get_value_int(0);
					}
					else if(ini.is_value("availablemodules"))
					{
						carrierInfo.iDockingModules = ini.get_value_int(0);
					}
				}
				if (!shipClientId)
					continue;

				mobiledockClients[shipClientId] = carrierInfo;
				ConPrint(L"Loaded Carrier\n");

				// Delete the file now that it's been loaded
				ConPrint(L"I should be deleting path: %s\n", stows(tpath));
				remove(tpath);
			}
		}
		ini.close();
	}

	sprintf(tpath, R"(%s\Accts\MultiPlayer\docking_module\docked_%08x.ini)", datapath, clientID);
	path = tpath;
	if(ini.open(path.c_str(), false))
	{
		while(ini.read_header())
		{
			if(ini.is_header("Ship"))
			{
				uint shipClientId;
				CLIENT_DATA shipInfo;

				while(ini.read_value())
				{
					if(ini.is_value("clientid"))
					{
						shipClientId = ini.get_value_int(0);
					}
					else if(ini.is_value("dockedwith"))
					{
						shipInfo.wscDockedWithCharname = stows(ini.get_value_string(0));
					}
					else if(ini.is_value("lastdockedbase"))
					{
						shipInfo.iLastBaseID = ini.get_value_int(0);
					}
				}

				if (!shipClientId)
					continue;
				mobiledockClients[shipClientId] = shipInfo;
				ConPrint(L"Loaded Ship\n");

				// Delete the file now that it's been loaded
				ConPrint(L"Attempting to delete path: %u\n", path.c_str());
				remove(path.c_str());
			}
		}
	}

}