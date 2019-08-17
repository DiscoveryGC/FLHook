#include "Database.h"

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
		for (vector<MODULE_CACHE>::iterator it = modules.begin(); it != modules.end(); it++)
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

		for (vector<MODULE_CACHE>::iterator it = modules.begin(); it != modules.end(); it++)
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
		vector<string> strChars = GetParams(HookExt::IniGetS(iClientID, "DockedChars"), '|');
		for (vector<string>::iterator it = strChars.begin(); it != strChars.end(); it++)
		{
			uint archID = boost::lexical_cast<uint>(GetParam(*it, ',', 0));
			wstring occupiedBy = EncodeWStringFromStringOfBytes(GetParam(*it, ',', 1));
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

		if(!str.empty())
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
				if(toErase.length() != str.size() && str[toErase.length()] != '|')
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
						if(pos - index == 0)
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
		map<string, vector<string>> fields;
		fields["system"];
		fields["base"];
		fields["pos"];
		fields["rotate"];
		fields["cargo"];
		fields["DockedChars"];
		fields["DockedWith"];
		fields["DockedToModule"];
		fields["saveLastBaseID"];
		fields["saveLastPOBID"];
		fields["base.player_base"];
		fields["base.last_player_base"];

		string path = GetFLAccPath(charname);
		bool foundFile = ReadFLFile(fields, path);

		if (foundFile)
		{
			Charname = charname;

			if (!fields["system"].empty())
				Location.systemID = CreateID(Trim(fields["system"][0]).c_str());
			else
				Location.systemID = 0;

			if (!fields["base"].empty())
				Location.baseID = CreateID(Trim(fields["base"][0]).c_str());
			else
				Location.baseID = 0;

			if (!fields["pos"].empty() && !fields["rotate"].empty())
			{
				Location.pos.x = boost::lexical_cast<float>(Trim(GetParam(fields["pos"][0], ',', 0)));
				Location.pos.y = boost::lexical_cast<float>(Trim(GetParam(fields["pos"][0], ',', 1)));
				Location.pos.z = boost::lexical_cast<float>(Trim(GetParam(fields["pos"][0], ',', 2)));

				Location.rot.x = boost::lexical_cast<float>(Trim(GetParam(fields["rotate"][0], ',', 0)));
				Location.rot.y = boost::lexical_cast<float>(Trim(GetParam(fields["rotate"][0], ',', 1)));
				Location.rot.z = boost::lexical_cast<float>(Trim(GetParam(fields["rotate"][0], ',', 2)));
			}
			else
			{
				Location.pos = Vector{ 0,100000,0 };
				Location.rot = Vector{ 0,0,0 };
			}

			vector<CARGO_ITEM> cargo;
			for (vector<string>::iterator it = fields["cargo"].begin(); it != fields["cargo"].end(); it++)
			{
				uint archID = boost::lexical_cast<uint>(Trim(GetParam(*it, ',', 0)));
				uint count = boost::lexical_cast<uint>(Trim(GetParam(*it, ',', 1)));
				cargo.push_back(CARGO_ITEM(archID, count));
			}
			Cargo = CargoDB(cargo);

			if (!fields["DockedChars"].empty())
			{
				vector<string> strData = GetParams(fields["DockedChars"][0], '|');
				for (vector<string>::iterator it = strData.begin(); it != strData.end(); it++)
				{
					uint archID = boost::lexical_cast<uint>(GetParam(*it, ',', 0));
					wstring occupiedBy = EncodeWStringFromStringOfBytes(GetParam(*it, ',', 1));
					DockedChars.push_back(MODULE_CACHE(archID, occupiedBy));
				}
			}

			if (!fields["DockedWith"].empty())
			{
				DockedWith = EncodeWStringFromStringOfBytes(fields["DockedWith"][0]);
				IsDocked = true;
			}
			else
				IsDocked = false;

			if (!fields["DockedToModule"].empty())
				DockedToModule = boost::lexical_cast<uint>(fields["DockedToModule"][0]);
			else
				DockedToModule = 0;

			if (!fields["saveLastBaseID"].empty())
				saveLastBaseID = boost::lexical_cast<uint>(fields["saveLastBaseID"][0]);
			else
				saveLastBaseID = 0;

			if (!fields["saveLastPOBID"].empty())
				saveLastPOBID = boost::lexical_cast<uint>(fields["saveLastPOBID"][0]);
			else
				saveLastPOBID = 0;

			if (!fields["base.player_base"].empty())
				POBID = boost::lexical_cast<uint>(fields["base.player_base"][0]);
			else
				POBID = 0;

			if (!fields["base.last_player_base"].empty())
				LastPOBID = boost::lexical_cast<uint>(fields["base.last_player_base"][0]);
			else
				LastPOBID = 0;
		}
	}

	void OfflineData::Save()
	{
		vector<string> linesToDelete;
		linesToDelete.push_back("base =");
		linesToDelete.push_back("pos =");
		linesToDelete.push_back("rotate =");
		linesToDelete.push_back("DockedChars");
		linesToDelete.push_back("DockedWith");
		linesToDelete.push_back("saveLastBaseID");
		linesToDelete.push_back("saveLastPOBID");
		linesToDelete.push_back("DockedToModule");
		linesToDelete.push_back("base.player_base");
		linesToDelete.push_back("base.last_player_base");

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

		vector<string> linesToAdd;
		if (!DockedWith.empty())
			linesToAdd.push_back("DockedWith = " + DecodeWStringToStringOfBytes(DockedWith));
		if (DockedToModule)
			linesToAdd.push_back("DockedToModule = " + to_string(DockedToModule));
		if (saveLastBaseID)
			linesToAdd.push_back("saveLastBaseID = " + to_string(saveLastBaseID));
		if (saveLastPOBID)
			linesToAdd.push_back("saveLastPOBID = " + to_string(saveLastPOBID));
		if (POBID)
			linesToAdd.push_back("base.player_base = " + to_string(POBID));
		if (LastPOBID)
			linesToAdd.push_back("base.last_player_base = " + to_string(LastPOBID));

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

			linesToAdd.push_back("DockedChars = " + SetParams(strDockedChars, '|'));
		}
		
		string path = GetFLAccPath(Charname);
		EditFLFile(&linesToDelete, &linesToAdd, &linesToReplace, path);
	}
	// End of OfflineData class.
}