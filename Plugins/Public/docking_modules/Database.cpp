#include "Database.h"

string dataPath;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Definitions
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace DB
{
	// Instance of WatcherDB.
	WatcherDB Watcher;

	// Instance of ClientsDB.
	ClientsDB Clients;

	// Begin of WatcherDB class.

	void WatcherDB::ReleaseModule(uint carrierClientID, wstring& dockedCharname)
	{
		vector<MODULE_CACHE>& modules = Cache[carrierClientID].Modules;
		for (vector<MODULE_CACHE>::iterator it = modules.begin(); it != modules.end(); ++it)
		{
			if (it->occupiedBy == dockedCharname)
			{
				Clients[carrierClientID].DockedChars.Erase(it);
				it->occupiedBy = L"";
				return;
			}
		}
	}

	void WatcherDB::OccupyModule(uint carrierClientID, uint moduleArch, wstring& dockingCharname)
	{
		vector<MODULE_CACHE> &modules = Cache[carrierClientID].Modules;

		for (vector<MODULE_CACHE>::iterator it = modules.begin(); it != modules.end(); ++it)
		{
			if (it->archID == moduleArch && it->occupiedBy.empty())
			{
				Clients[carrierClientID].DockedChars.Add(MODULE_CACHE(it->archID, dockingCharname));
				it->occupiedBy = dockingCharname;
				return;
			}
		}
	}
	// End of WatcherDB class.

	// Begin of DockedChars class.

	const vector<MODULE_CACHE> DockedCharsDB::Get()
	{
		vector<MODULE_CACHE> Chars;
		for (string &str : GetParams(HookExt::IniGetS(iClientID, "DockedChars"), '|'))
		{
			uint archID = boost::lexical_cast<uint>(GetParam(str, ',', 0));
			wstring occupiedBy = EncodeWStringFromStringOfBytes(GetParam(str, ',', 1));
			Chars.push_back(MODULE_CACHE(archID, occupiedBy));
		}

		return Chars;
	}

	bool DockedCharsDB::Empty()
	{
		return HookExt::IniGetS(iClientID, "DockedChars").empty();
	}

	void DockedCharsDB::Clear()
	{
		HookExt::IniSetS(iClientID, "DockedChars", "");
	}

	void DockedCharsDB::Add(MODULE_CACHE &info)
	{
		string str = HookExt::IniGetS(iClientID, "DockedChars");

		if (!str.empty())
			str += '|';
		str += to_string(info.archID);
		str += ',';
		str += DecodeWStringToStringOfBytes(info.occupiedBy);

		HookExt::IniSetS(iClientID, "DockedChars", str);
	}

	void DockedCharsDB::Erase(vector<MODULE_CACHE>::iterator it)
	{
		// Search for the substring in string
		string str = HookExt::IniGetS(iClientID, "DockedChars");
		string toErase = to_string(it->archID) + ',' + DecodeWStringToStringOfBytes(it->occupiedBy);

		uint pos = str.find(toErase);
		if (pos != std::string::npos)
		{
			if (pos)
				str.erase(pos - 1, toErase.length() + 1);
			else
				if (toErase.length() != str.size() && str[toErase.length()] != '|')
					str.erase(pos, toErase.length());
				else
					str.erase(pos, toErase.length() + 1);

			HookExt::IniSetS(iClientID, "DockedChars", str);
		}
	}

	void DockedCharsDB::Remove(wstring &charname)
	{
		// Search for the substring in string
		string str = HookExt::IniGetS(iClientID, "DockedChars");
		string toErase = DecodeWStringToStringOfBytes(charname);

		uint pos = 0;
		bool notFound = true;
		while (notFound)
		{
			pos = str.find(toErase, pos);
			if (pos != string::npos && (pos + toErase.length() != str.size() && str[pos + toErase.length()] != '|'))
			{
				for (uint index = 0; ; index++)
				{
					if (str[pos - index] == '|' || pos - index == 0)
					{
						if (pos - index == 0)
							str.erase(pos - index, toErase.length() + index);
						else
							str.erase(pos - index, toErase.length() + 1);

						HookExt::IniSetS(iClientID, "DockedChars", str);
						notFound = false;
						break;
					}
				}
			}
		}
	}
	// End of DockedChars class.

	// Begin of OfflineData class.
	OfflineData::OfflineData(wstring& charname)
	{
		map<string, vector<string>> variables;
		variables["system"];
		variables["base"];
		variables["pos"];
		variables["rotate"];
		variables["cargo"];

		map<string, string> hookExtData;
		hookExtData["DockedChars"];
		hookExtData["DockedWith"];
		hookExtData["DockedToModule"];
		hookExtData["saveLastBaseID"];
		hookExtData["saveLastPOBID"];
		hookExtData["base.player_base"];
		hookExtData["base.last_player_base"];

		path = GetFLAccPath(charname);
		ReadFLFile(variables, hookExtData, path);


		Charname = charname;

		if (!variables["system"].empty())
			Location.systemID = CreateID(Trim(variables["system"][0]).c_str());
		else
			Location.systemID = 0;

		if (!variables["base"].empty())
			Location.baseID = CreateID(Trim(variables["base"][0]).c_str());
		else
			Location.baseID = 0;

		if (!variables["pos"].empty() && !variables["rotate"].empty())
		{
			Location.pos.x = boost::lexical_cast<float>(Trim(GetParam(variables["pos"][0], ',', 0)));
			Location.pos.y = boost::lexical_cast<float>(Trim(GetParam(variables["pos"][0], ',', 1)));
			Location.pos.z = boost::lexical_cast<float>(Trim(GetParam(variables["pos"][0], ',', 2)));

			Location.rot.x = boost::lexical_cast<float>(Trim(GetParam(variables["rotate"][0], ',', 0)));
			Location.rot.y = boost::lexical_cast<float>(Trim(GetParam(variables["rotate"][0], ',', 1)));
			Location.rot.z = boost::lexical_cast<float>(Trim(GetParam(variables["rotate"][0], ',', 2)));
		}
		else
		{
			Location.pos = Vector{ 0,100000,0 };
			Location.rot = Vector{ 0,0,0 };
		}

		vector<CARGO_ITEM> cargo;
		for (vector<string>::iterator it = variables["cargo"].begin(); it != variables["cargo"].end(); it++)
		{
			uint archID = boost::lexical_cast<uint>(Trim(GetParam(*it, ',', 0)));
			uint count = boost::lexical_cast<uint>(Trim(GetParam(*it, ',', 1)));
			cargo.push_back(CARGO_ITEM(archID, count));
		}
		Cargo = CargoDB(cargo);

		if (!variables["DockedChars"].empty())
		{
			vector<string> strData = GetParams(variables["DockedChars"][0], '|');
			for (vector<string>::iterator it = strData.begin(); it != strData.end(); it++)
			{
				uint archID = boost::lexical_cast<uint>(GetParam(*it, ',', 0));
				wstring occupiedBy = EncodeWStringFromStringOfBytes(GetParam(*it, ',', 1));
				DockedChars.push_back(MODULE_CACHE(archID, occupiedBy));
			}
		}

		if (!variables["DockedWith"].empty())
		{
			DockedWith = EncodeWStringFromStringOfBytes(variables["DockedWith"][0]);
			IsDocked = true;
		}
		else
			IsDocked = false;

		if (!variables["DockedToModule"].empty())
			DockedToModule = boost::lexical_cast<uint>(variables["DockedToModule"][0]);
		else
			DockedToModule = 0;

		if (!variables["saveLastBaseID"].empty())
			saveLastBaseID = boost::lexical_cast<uint>(variables["saveLastBaseID"][0]);
		else
			saveLastBaseID = 0;

		if (!variables["saveLastPOBID"].empty())
			saveLastPOBID = boost::lexical_cast<uint>(variables["saveLastPOBID"][0]);
		else
			saveLastPOBID = 0;

		if (!variables["base.player_base"].empty())
			POBID = boost::lexical_cast<uint>(variables["base.player_base"][0]);
		else
			POBID = 0;

		if (!variables["base.last_player_base"].empty())
			LastPOBID = boost::lexical_cast<uint>(variables["base.last_player_base"][0]);
		else
			LastPOBID = 0;
	}

	void OfflineData::Save()
	{
		vector<string> linesToDelete;
		linesToDelete.push_back("base =");
		linesToDelete.push_back("pos =");
		linesToDelete.push_back("rotate =");

		vector<string> hookExtLinesToDelete;
		hookExtLinesToDelete.push_back("DockedChars");
		hookExtLinesToDelete.push_back("DockedWith");
		hookExtLinesToDelete.push_back("DockedToModule");
		hookExtLinesToDelete.push_back("saveLastBaseID");
		hookExtLinesToDelete.push_back("saveLastPOBID");
		hookExtLinesToDelete.push_back("base.player_base");
		hookExtLinesToDelete.push_back("base.last_player_base");

		if (Cargo.Get().empty())
			linesToDelete.push_back("cargo =");

		map<string, vector<string>> linesToReplace;
		if (Location.baseID)
			linesToReplace["system"] =
		{
			"system = " + wstos(HkGetSystemNickByID(Location.systemID)),
			"base = " + wstos(HkGetBaseNickByID(Location.baseID))
		};
		else
			linesToReplace["system"] =
		{
			"system = " + wstos(HkGetSystemNickByID(Location.systemID)),
			"pos = " + to_string(Location.pos.x) + "," + to_string(Location.pos.y) + "," + to_string(Location.pos.z),
			"rotate = " + to_string(Location.rot.x) + "," + to_string(Location.rot.y) + "," + to_string(Location.rot.z)
		};

		vector<string> hookExtLinesToAdd;
		if (!DockedWith.empty())
			hookExtLinesToAdd.push_back("DockedWith = " + DecodeWStringToStringOfBytes(DockedWith));
		if (DockedToModule)
			hookExtLinesToAdd.push_back("DockedToModule = " + to_string(DockedToModule));
		if (saveLastBaseID)
			hookExtLinesToAdd.push_back("saveLastBaseID = " + to_string(saveLastBaseID));
		if (saveLastPOBID)
			hookExtLinesToAdd.push_back("saveLastPOBID = " + to_string(saveLastPOBID));
		if (POBID)
			hookExtLinesToAdd.push_back("base.player_base = " + to_string(POBID));
		if (LastPOBID)
			hookExtLinesToAdd.push_back("base.last_player_base = " + to_string(LastPOBID));

		if (!DockedChars.empty())
		{
			vector<string> strDockedChars;
			for (vector<MODULE_CACHE>::iterator it = DockedChars.begin(); it != DockedChars.end(); it++)
			{
				string str;
				str += to_string(it->archID);
				str += ",";
				str += DecodeWStringToStringOfBytes(it->occupiedBy);
				strDockedChars.push_back(str);
			}

			hookExtLinesToAdd.push_back("DockedChars = " + SetParams(strDockedChars, '|'));
		}

		EditFLFile(linesToDelete, linesToReplace, hookExtLinesToAdd, hookExtLinesToDelete, path);
	}
	// End of OfflineData class.
}