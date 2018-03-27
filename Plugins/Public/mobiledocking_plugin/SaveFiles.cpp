#include "Main.h"

// A function used to save a docked player to the filesystem
void SaveDockInfoCarried(const wstring& shipFileName, uint clientID, const CLIENT_DATA& client)
{
	// The path to the data file.
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	char tpath[1024];
	sprintf(tpath, R"(%s\Accts\MultiPlayer\docking_module\docked_%ls.ini)", datapath, shipFileName.c_str());
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
void SaveDockInfoCarrier(const wstring& shipFileName, uint clientID, const CLIENT_DATA& client)
{
	// The path to the data file.
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	char tpath[1024];
	sprintf(tpath, R"(%s\Accts\MultiPlayer\docking_module\carrier_%ls.ini)", datapath, shipFileName.c_str());
	string path = tpath;

	FILE *file = fopen(path.c_str(), "w");
	if(file)
	{
		fprintf(file, "[Carrier]\n");
		fprintf(file, "clientid=%u\n", clientID);
		
		// Save each docked ship name
		for (map<wstring,wstring>::const_iterator it = client.mapDockedShips.begin(); it != client.mapDockedShips.end(); ++it)
		{
			fprintf(file, "dockedchar=%ls, %ls\n", it->first.c_str(), it->second.c_str());
		}

		fprintf(file, "lastdocked=%u\n", client.iLastBaseID);
		fprintf(file, "availablemodules=%u\n", mobiledockClients[clientID].iDockingModulesAvailable);
	}
	fclose(file);
}

void LoadShip(string shipFileName)
{
	// The path to the data file.
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	char tpath[1024];
	sprintf(tpath, R"(%s\Accts\MultiPlayer\docking_module\carrier_%s.ini)", datapath, shipFileName.c_str());
	string path = tpath;

	INI_Reader ini;

	bool foundFile = false;

	// Attempt to load the ship as a carrier
	if (ini.open(path.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("Carrier"))
			{
				uint shipClientId;
				CLIENT_DATA carrierInfo;
				foundFile = true;

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
					else if (ini.is_value("availablemodules"))
					{
						carrierInfo.iDockingModulesAvailable = ini.get_value_int(0);
					}
				}
				if (!shipClientId)
					continue;

				mobiledockClients[shipClientId] = carrierInfo;
			}
		}
		ini.close();

		if (foundFile) {
			int error = _unlink(path.c_str());
			return;
		}
	}

	sprintf(tpath, R"(%s\Accts\MultiPlayer\docking_module\docked_%s.ini)", datapath, shipFileName.c_str());
	path = tpath;

	if(ini.open(path.c_str(), false))
	{
		while(ini.read_header())
		{
			if(ini.is_header("Ship"))
			{
				uint shipClientId;
				CLIENT_DATA shipInfo;
				foundFile = true;

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

				shipInfo.mobileDocked = true;
				mobiledockClients[shipClientId] = shipInfo;
			}
		}
	}

	ini.close();
	
	if (foundFile) {
		_unlink(path.c_str());
	}

}