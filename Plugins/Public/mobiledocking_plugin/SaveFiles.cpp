#include "Main.h"

void LoadDockInfo(uint clientID)
{
	CLIENT_DATA &cd = clients[clientID];
	cd.mapDockedShips.clear();

	// The path to the data file.
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	char tpath[1024];
	sprintf(tpath, R"(%s\Accts\MultiPlayer\docked_players\docked_%08x.ini)", datapath, clientID);
	string path = tpath;

	INI_Reader ini;
	if(ini.open(path.c_str(), false))
	{
		while(ini.read_header())
		{
			if(ini.is_header("DOCK"))
			{
				while(ini.read_value())
				{
					if(ini.is_value("dockedwith"))
					{
						cd.wscDockedWithCharname = stows(ini.get_value_string(0));
					}
					else if(ini.is_value("lastdockedbase"))
					{
						cd.iLastBaseID = static_cast<uint>(ini.get_value_int(0));
					}
				}
			}
		}
		ini.close();
	}
}

void SaveDockInfo(CLIENT_DATA client, uint clientID)
{
	// The path to the data file.
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	char tpath[1024];
	sprintf(tpath, R"(%s\Accts\MultiPlayer\docked_players\docked_%08x.ini)", datapath, clientID);
	string path = tpath;

	FILE *file = fopen(path.c_str(), "w");
	if(file)
	{
		fprintf(file, "[DOCK]\n");
		fprintf(file, "dockedwith=%ls\n", client.wscDockedWithCharname.c_str());
		fprintf(file, "lastdockedbase=%u\n", client.iLastBaseID);
	}

	fclose(file);
}

void SavePluginState()
{
	// The path to the data file.
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	char tpath[1024];
	sprintf(tpath, R"(%s\Accts\MultiPlayer\docked_players\plugin_state.ini)", datapath);
	string path = tpath;

	FILE *file = fopen(path.c_str(), "w");
	if(file)
	{
		fprintf(file, "[DockedClients]\n");

		// Save each clientID to the file.
		for (auto& client : clients)
		{
			fprintf(file, "dockedclient=%u\n", client.first);
		}
	}
	fclose(file);
}

void LoadPluginState()
{
	// The path to the data file.
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	char tpath[1024];
	sprintf(tpath, R"(%s\Accts\MultiPlayer\docked_players\plugin_state.ini)", datapath);
	string path = tpath;

	INI_Reader ini;
	if(ini.open(path.c_str(), false))
	{
		while(ini.read_header())
		{
			if(ini.is_header("DockedClients"))
			{
				if(ini.is_value("dockedclient"))
				{
					LoadDockInfo(static_cast<uint>(ini.get_value_int(0)));
				}
			}
		}
	}
}