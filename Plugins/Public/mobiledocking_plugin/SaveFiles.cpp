#include "Main.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>

// Standard crate archetype.
uint set_iLootCrateID = CreateID("lootcrate_ast_loot_metal");

// Checks if file exists.
inline bool CheckIfExists(const string& path)
{
	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0);
}

// A function used to save a docked player to the filesystem.
void SaveDockInfoCarried(wstring& charname, uint clientID, const CLIENT_DATA& client)
{
	// The path to the data file.
	string path = GetSavePath(charname, false);

	vector<string> linesToAdd;

	linesToAdd.push_back("lastdockedbase=" + to_string(client.iLastBaseID));
	linesToAdd.push_back("proxybase=" + to_string(client.proxyBaseID));

	// If carrier is alive - write its name.
	if(!client.wscDockedWithCharname.empty())
		linesToAdd.push_back("dockedwith=" + wstos(client.wscDockedWithCharname));
	else
	// Otherwise, write its last known position.
	{
		Vector carrierRot = MatrixToEuler(client.carrierRot);
		linesToAdd.push_back("system=" + to_string(client.carrierSystem));
		linesToAdd.push_back("posX=" + to_string(client.carrierPos.x));
		linesToAdd.push_back("posY=" + to_string(client.carrierPos.y));
		linesToAdd.push_back("posZ=" + to_string(client.carrierPos.z));
		linesToAdd.push_back("rotX=" + to_string(carrierRot.x));
		linesToAdd.push_back("rotY=" + to_string(carrierRot.y));
		linesToAdd.push_back("rotZ=" + to_string(carrierRot.z));
	}

	// Save data to ini file.
	EditFLFile(nullptr, &linesToAdd, nullptr, path, true);
}

// A function used to save a carrier player to the filesystem.
void SaveDockInfoCarrier(wstring& charname, uint clientID, const CLIENT_DATA& client)
{
	// The path to the data file.
	string path = GetSavePath(charname, true);

	vector<string> linesToAdd;

	// Save each docked ship name.
	for (vector<wstring>::const_iterator it = client.dockedShips.begin(); it != client.dockedShips.end(); ++it)
	{
		linesToAdd.push_back("dockedchar=" + wstos((*it)));
	}

	// If carrier is at POB now, save his last known coordinates in space, because plugin can't handle undocking to POBs for docked ships.
	if (mobiledockClients[clientID].iLastBaseID == GetProxyBaseForClient(clientID))
	{
		Vector carrierRot = MatrixToEuler(mobiledockClients[clientID].carrierRot);
		linesToAdd.push_back("posX=" + to_string(mobiledockClients[clientID].carrierPos.x));
		linesToAdd.push_back("posY=" + to_string(mobiledockClients[clientID].carrierPos.y));
		linesToAdd.push_back("posZ=" + to_string(mobiledockClients[clientID].carrierPos.z));
		linesToAdd.push_back("rotX=" + to_string(carrierRot.x));
		linesToAdd.push_back("rotY=" + to_string(carrierRot.y));
		linesToAdd.push_back("rotZ=" + to_string(carrierRot.z));
	}

	// Save data to ini file.
	EditFLFile(nullptr, &linesToAdd, nullptr, path, true);
}

// Try to load plugin data for this ship from save file if it exists.
void LoadShip(string& shipFileName, uint shipClientId)
{
	CLIENT_DATA shipInfo;

	// Attempt to load the ship as a carrier.
	string path = GetSavePath(shipFileName, true);
	map<string, vector<string>> fields;
	fields["dockedchar"].push_back("=");
	fields["posX"].push_back("=");
	fields["posY"].push_back("=");
	fields["posZ"].push_back("=");
	fields["rotX"].push_back("=");
	fields["rotY"].push_back("=");
	fields["rotZ"].push_back("=");
	ReadFLFile(fields, path);

	if (fields["posX"].size() > 1 && fields["posY"].size() > 1 && fields["posZ"].size() > 1)
	{
		shipInfo.carrierSystem = Players[shipClientId].iSystemID;
		shipInfo.carrierPos.x = (float)atof(fields["posX"][1].c_str());
		shipInfo.carrierPos.y = (float)atof(fields["posY"][1].c_str());
		shipInfo.carrierPos.z = (float)atof(fields["posZ"][1].c_str());
	}

	if (fields["rotX"].size() > 1 && fields["rotY"].size() > 1 && fields["rotZ"].size() > 1)
	{
		Vector eulerCarrierRot;
		eulerCarrierRot.x = (float)atof(fields["rotX"][1].c_str());
		eulerCarrierRot.y = (float)atof(fields["rotY"][1].c_str());
		eulerCarrierRot.z = (float)atof(fields["rotZ"][1].c_str());
		shipInfo.carrierRot = EulerMatrix(eulerCarrierRot);
	}

	if (fields["dockedchar"].size() > 1)
	{
		for (uint i = 1; i < fields["dockedchar"].size(); i++)
		{
			shipInfo.dockedShips.push_back(stows(fields["dockedchar"][i]));
		}

		shipInfo.carrierSystem = Players[shipClientId].iSystemID;
		shipInfo.iLastBaseID = Players[shipClientId].iLastBaseID;
		mobiledockClients[shipClientId] = shipInfo;
		int error = _unlink(path.c_str());
		return;
	}

	// Attempt to load ship as docked.
	path = GetSavePath(shipFileName, false);
	fields.clear();
	fields["dockedwith"].push_back("=");
	fields["lastdockedbase"].push_back("=");
	fields["proxybase"].push_back("=");
	fields["system"].push_back("=");
	fields["posX"].push_back("=");
	fields["posY"].push_back("=");
	fields["posZ"].push_back("=");
	fields["rotX"].push_back("=");
	fields["rotY"].push_back("=");
	fields["rotZ"].push_back("=");
	bool foundFile = ReadFLFile(fields, path);

	if (fields["lastdockedbase"].size() > 1)
		stringstream(fields["lastdockedbase"][1]) >> shipInfo.iLastBaseID;

	if (fields["proxybase"].size() > 1)
		stringstream(fields["proxybase"][1]) >> shipInfo.proxyBaseID;

	if (fields["dockedwith"].size() > 1)
		shipInfo.wscDockedWithCharname = stows(fields["dockedwith"][1]);

	if (fields["system"].size() > 1)
		stringstream(fields["system"][1]) >> shipInfo.carrierSystem;

	if (fields["posX"].size() > 1 && fields["posY"].size() > 1 && fields["posZ"].size() > 1)
	{
		shipInfo.carrierSystem = Players[shipClientId].iSystemID;
		shipInfo.carrierPos.x = (float)atof(fields["posX"][1].c_str());
		shipInfo.carrierPos.y = (float)atof(fields["posY"][1].c_str());
		shipInfo.carrierPos.z = (float)atof(fields["posZ"][1].c_str());
	}

	if (fields["rotX"].size() > 1 && fields["rotY"].size() > 1 && fields["rotZ"].size() > 1)
	{
		Vector eulerCarrierRot;
		eulerCarrierRot.x = (float)atof(fields["rotX"][1].c_str());
		eulerCarrierRot.y = (float)atof(fields["rotY"][1].c_str());
		eulerCarrierRot.z = (float)atof(fields["rotZ"][1].c_str());
		shipInfo.carrierRot = EulerMatrix(eulerCarrierRot);
	}

	if (foundFile)
	{
		mobiledockClients[shipClientId] = shipInfo;
		int error = _unlink(path.c_str());
	}
}

// Returns position of offline carrier ship. Automatically removes ship of this clientID from list of docked characters.
SHIP_LOCATION GetCarrierPosOffline(uint dockedClientID, wstring dockedCharname)
{
	SHIP_LOCATION location;
	wstring charName;

	if (dockedClientID)
		charName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(dockedClientID));
	else
		charName = dockedCharname;

	string path = GetSavePath(mobiledockClients[dockedClientID].wscDockedWithCharname, true);

	map<string, vector<string>> fields;
	fields["dockedchar"].push_back("=");
	fields["posX"].push_back("=");
	fields["posY"].push_back("=");
	fields["posZ"].push_back("=");
	fields["rotX"].push_back("=");
	fields["rotY"].push_back("=");
	fields["rotZ"].push_back("=");
	bool foundFile = ReadFLFile(fields, path);

	int dockedCharCount = 0;

	if (fields["dockedchar"].size() > 1)
	for (uint i = 1; i < fields["dockedchar"].size(); i++)
	{
		dockedCharCount++;
		if (fields["dockedchar"][i] == wstos(charName))
			dockedCharCount--;
	}

	// If ship docked at POB, load its last known position in space, because plugin can't handle undocking ships directly to POBs.
	if (fields["posX"].size() > 1 && fields["posY"].size() > 1 && fields["posZ"].size() > 1)
	{
		location.pos.x = (float)atof(fields["posX"][1].c_str());
		location.pos.y = (float)atof(fields["posY"][1].c_str());
		location.pos.z = (float)atof(fields["posZ"][1].c_str());
	}

	if (fields["rotX"].size() > 1 && fields["rotY"].size() > 1 && fields["rotZ"].size() > 1)
	{
		location.rot.x = (float)atof(fields["rotX"][1].c_str());
		location.rot.y = (float)atof(fields["rotY"][1].c_str());
		location.rot.z = (float)atof(fields["rotZ"][1].c_str());
	}

	string accPath = GetFLAccPath(mobiledockClients[dockedClientID].wscDockedWithCharname);

	fields.clear();
	fields["system"].push_back("=");
	fields["base"].push_back("=");
	fields["pos"].push_back("=");
	fields["rotate"].push_back("=");
	ReadFLFile(fields, accPath);

	if (fields["system"].size() > 1)
	{
		pub::GetSystemID(location.systemID, fields["system"][1].c_str());
	}
	if (fields["base"].size() > 1)
	{
		pub::GetBaseID(location.baseID, fields["base"][1].c_str());
	}
	else if (fields["pos"].size() > 1)
	{
		location.pos.x = (float)atof(Trim(GetParam(fields["pos"][1], ',', 0)).c_str());
		location.pos.y = (float)atof(Trim(GetParam(fields["pos"][1], ',', 1)).c_str());
		location.pos.z = (float)atof(Trim(GetParam(fields["pos"][1], ',', 2)).c_str());
	}

	else if (fields["rotate"].size() > 1)
	{
		location.rot.x = (float)atof(Trim(GetParam(fields["rotate"][1], ',', 0)).c_str());
		location.rot.y = (float)atof(Trim(GetParam(fields["rotate"][1], ',', 1)).c_str());
		location.rot.z = (float)atof(Trim(GetParam(fields["rotate"][1], ',', 2)).c_str());
	}

	// Check if carrier docked to POB - nullify base ID.
	if (location.baseID == GetProxyBaseForSystem(location.systemID))
		location.baseID = 0;

	// If none is docked anymore - remove the file
	if (foundFile && dockedCharCount == 0)
	{
		_unlink(path.c_str());
	}
	// If we have someone else docked, remove current ship from docked list
	else if (foundFile && !charName.empty())
	{
		vector<string> linesToDelete = {"dockedchar=" + wstos(charName) };
		EditFLFile(&linesToDelete, nullptr, nullptr, path, false, true);
	}

	return location;
}

// Jettisons offline player from online carrier.
void JettisonShipOffline(uint carrierClientID, wstring dockedCharname)
{
	wstring carrierCharname = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(carrierClientID));

	string path = GetSavePath(dockedCharname, false);

	string accPath = GetFLAccPath(dockedCharname);

	map<string, vector<string>> linesToReplace;
	vector<string> replaceWith;
	vector<string> linesToDelete = {"base = ", "pos = ", "rotate = " };

	map<string, vector<string>> fields;
	fields["lastdockedbase"].push_back("=");
	ReadFLFile(fields, path);

	if (fields["lastdockedbase"].size() > 1)
	{
		// Update last base to prevent bugs.
		uint newBaseID;
		stringstream(fields["lastdockedbase"][1]) >> newBaseID;
		linesToReplace["last_base"] = { "last_base = " + wstos(HkGetBaseNickByID(newBaseID)) };

		// Remove dock save file, because ship is jettisoned and no more part of this plugin
		_unlink(path.c_str());
	}

	// If carrier docked at real base, jettison ship at that base, update base ID in file
	if (Players[carrierClientID].iBaseID != GetProxyBaseForClient(carrierClientID) && Players[carrierClientID].iBaseID)
	{
		replaceWith.push_back("system = " + wstos(HkGetSystemNickByID(Players[carrierClientID].iSystemID)));
		replaceWith.push_back("base = " + wstos(HkGetBaseNickByID(Players[carrierClientID].iBaseID)));
	}
	// Else if carrier docked to PLAYER owned base, jettison ship to last known position in space.
	else if (Players[carrierClientID].iBaseID)
	{
		Vector pos = mobiledockClients[carrierClientID].carrierPos;
		Vector rot = MatrixToEuler(mobiledockClients[carrierClientID].carrierRot);
		replaceWith.push_back("system = " + wstos(HkGetSystemNickByID(Players[carrierClientID].iSystemID)));
		replaceWith.push_back("pos = " + to_string(pos.x) + "," + to_string(pos.y) + "," + to_string(pos.z));
		replaceWith.push_back("rotate = " + to_string(rot.x) + "," + to_string(rot.y) + "," + to_string(rot.z));
	}
	// If carrier is in space, jettison ship to current carrier position.
	else
	{
		uint carrierShip;
		pub::Player::GetShip(carrierClientID, carrierShip);

		Vector pos;
		Matrix ornt;

		pub::SpaceObj::GetLocation(carrierShip, pos, ornt);

		pos = GetUndockingPosition(pos, ornt);
		Vector rot = MatrixToEuler(ornt);

		replaceWith.push_back("system = " + wstos(HkGetSystemNickByID(Players[carrierClientID].iSystemID)));
		replaceWith.push_back("pos = " + to_string(pos.x) + "," + to_string(pos.y) + "," + to_string(pos.z));
		replaceWith.push_back("rotate = " + to_string(rot.x) + "," + to_string(rot.y) + "," + to_string(rot.z));
	}

	linesToReplace["system"] = replaceWith;

	EditFLFile(&linesToDelete, nullptr, &linesToReplace, accPath);

	// Play jettisoning sound and notify carrier.
	pub::Audio::PlaySoundEffect(carrierClientID, CreateID("cargo_jettison"));
	PrintUserCmdText(carrierClientID, L"Ship jettisoned.");
}

// Overloaded function, call it if need to jettison offline ship from offline carrier.
void JettisonShipOffline(wstring dockedCharname, wstring carrierCharname)
{
	SHIP_LOCATION location;

	string carrierpath = GetSavePath(carrierCharname, true);
	string dockedpath = GetSavePath(dockedCharname, false);

	string carrieraccPath = GetFLAccPath(carrierCharname);
	string dockedaccPath = GetFLAccPath(dockedCharname);

	map<string, vector<string>> fields;
	// Initialize fields.
	fields["system"].push_back("=");
	fields["base"].push_back("=");
	fields["pos"].push_back("=");
	fields["rotate"].push_back("=");
	// Read values, associated with these fields.
	ReadFLFile(fields, carrieraccPath);

	if (fields["system"].size() > 1)
	{
		pub::GetSystemID(location.systemID, fields["system"][1].c_str());
	}
	if (fields["base"].size() > 1)
	{
		pub::GetBaseID(location.baseID, fields["base"][1].c_str());
	}
	if (fields["pos"].size() > 1)
	{
		location.pos.x = (float)atof(Trim(GetParam(fields["pos"][1], ',', 0)).c_str());
		location.pos.y = (float)atof(Trim(GetParam(fields["pos"][1], ',', 1)).c_str());
		location.pos.z = (float)atof(Trim(GetParam(fields["pos"][1], ',', 2)).c_str());
	}
	if (fields["rotate"].size() > 1)
	{
		location.rot.x = (float)atof(Trim(GetParam(fields["rotate"][1], ',', 0)).c_str());
		location.rot.y = (float)atof(Trim(GetParam(fields["rotate"][1], ',', 1)).c_str());
		location.rot.z = (float)atof(Trim(GetParam(fields["rotate"][1], ',', 2)).c_str());
	}

	// If carrier docked to POB - nullify base ID and load last known coordinates in space
	if (location.baseID == GetProxyBaseForSystem(location.systemID))
	{
		location.baseID = 0;
		fields.clear();
		fields["posX"].push_back("=");
		fields["posY"].push_back("=");
		fields["posZ"].push_back("=");
		fields["rotX"].push_back("=");
		fields["rotY"].push_back("=");
		fields["rotZ"].push_back("=");
		ReadFLFile(fields, carrierpath);

		if (fields["posX"].size() > 1 && fields["posY"].size() > 1 && fields["posZ"].size() > 1)
		{
			location.pos.x = (float)atof(fields["posX"][1].c_str());
			location.pos.y = (float)atof(fields["posY"][1].c_str());
			location.pos.z = (float)atof(fields["posZ"][1].c_str());
		}

		if (fields["rotX"].size() > 1 && fields["rotY"].size() > 1 && fields["rotZ"].size() > 1)
		{
			location.rot.x = (float)atof(fields["rotX"][1].c_str());
			location.rot.y = (float)atof(fields["rotY"][1].c_str());
			location.rot.z = (float)atof(fields["rotZ"][1].c_str());
		}
	}

	// Remove ship from carrier's dock save file, because it is jettisoned.
	vector<string> carrierLinesToDelete = { "dockedchar=" + wstos(dockedCharname) };
	EditFLFile(&carrierLinesToDelete, nullptr, nullptr, carrierpath, false, true);

	map<string, vector<string>> dockedlinesToReplace;
	vector<string> dockedreplaceWith;
	vector<string> dockedlinesToDelete = { "base = ", "pos = ", "rotate = " };

	fields.clear();
	fields["lastdockedbase"].push_back("=");
	ReadFLFile(fields, dockedpath);

	if (fields["lastdockedbase"].size() > 1)
	{
		// Update last base to prevent bugs.
		uint newBaseID;
		stringstream(fields["lastdockedbase"][1]) >> newBaseID;
		dockedlinesToReplace["last_base"] = { "last_base = " + wstos(HkGetBaseNickByID(newBaseID)) };

		_unlink(dockedpath.c_str());
	}

	// If carrier docked at base, jettison ship at that base, update base ID in file.
	if (location.baseID)
	{
		dockedreplaceWith.push_back("system = " + wstos(HkGetSystemNickByID(location.systemID)));
		dockedreplaceWith.push_back("base = " + wstos(HkGetBaseNickByID(location.baseID)));
	}
	// If carrier is in space, jettison ship to current carrier position.
	else
	{
		location.pos = GetUndockingPosition(location.pos, EulerMatrix(location.rot));

		dockedreplaceWith.push_back("system = " + wstos(HkGetSystemNickByID(location.systemID)));
		dockedreplaceWith.push_back("pos = " + to_string(location.pos.x) + "," + to_string(location.pos.y) + "," + to_string(location.pos.z));
		dockedreplaceWith.push_back("rotate = " + to_string(location.rot.x) + "," + to_string(location.rot.y) + "," + to_string(location.rot.z));
	}

	dockedlinesToReplace["system"] = dockedreplaceWith;

	EditFLFile(&dockedlinesToDelete, nullptr, &dockedlinesToReplace, dockedaccPath);
}

// Use it to throw in space all cargo of offline player docked to carrier. Moves ship to last real base.
void ThrowCargoOffline(wstring charname, Vector carrierPos, uint carrierSystemID, uint moveToBaseID)
{
	string path = GetSavePath(charname, false);
	string accPath = GetFLAccPath(charname);

	map<string, vector<string>> fields;
	fields["cargo"].push_back("=");
	ReadFLFile(fields, accPath);

	vector<string> cargo = fields["cargo"];

	// Throw cargo in space by given pos.
	if (cargo.size() > 1)
	for (uint i = 1; i < fields["cargo"].size(); i++)
	{
		uint archID;
		stringstream(Trim(GetParam(cargo[i], ',', 0))) >> archID;
		uint count;
		stringstream(Trim(GetParam(cargo[i], ',', 1))) >> count;

		Server.MineAsteroid(carrierSystemID, carrierPos, set_iLootCrateID, archID, count, 0);
	}

	map<string, vector<string>> linesToReplace;

	// Move ship to last real base
	if (moveToBaseID == 0)
	{
		fields.clear();
		fields["lastdockedbase"].push_back("=");
		ReadFLFile(fields, path);

		if (fields["lastdockedbase"].size() > 1)
		{
			// Update last base, because ship is "died" in explosion.
			uint newBaseID;
			stringstream(fields["lastdockedbase"][1]) >> newBaseID;
			linesToReplace["base"] = { "base = " + wstos(HkGetBaseNickByID(newBaseID)) };
			linesToReplace["last_base"] = { "last_base = " + wstos(HkGetBaseNickByID(newBaseID)) };

			_unlink(path.c_str());
		}
	}
	else
	{
		linesToReplace["base"] = { "base = " + wstos(HkGetBaseNickByID(moveToBaseID)) };
	}


	// Remove cargo from save file.
	vector<string> linesToDelete = { "cargo" };

	EditFLFile(&linesToDelete, nullptr, &linesToReplace, accPath);
}

// Updates carrier position in save file for offline character to avoid possible bugs.
void UpdateDyingCarrierPos(wstring& charname, Vector carrierPos, Matrix carrierOrnt, uint carrierSystem)
{
	string path = GetSavePath(charname, false);
	Vector carrierRot = MatrixToEuler(carrierOrnt);

	// Remove dockedwith field, because carrier is dying.
	vector<string> linesToDelete = { "dockedwith=" };
	vector<string> linesToAdd = 
	{
		"system=" + to_string(carrierSystem),
		"posX=" + to_string(carrierPos.x),
		"posY=" + to_string(carrierPos.y),
		"posZ=" + to_string(carrierPos.z),
		"rotX=" + to_string(carrierRot.x),
		"rotY=" + to_string(carrierRot.y),
		"rotZ=" + to_string(carrierRot.z)
	};

	EditFLFile(&linesToDelete, &linesToAdd, nullptr, path);
}

// Updates last base for docked ship if it is offline.
void UpdateLastBaseOffline(wstring& dockedCharname, uint lastBaseID)
{
	string path = GetSavePath(dockedCharname, false);

	map<string, vector<string>> linesToReplace;
	linesToReplace["lastdockedbase"] = { "lastdockedbase=" + to_string(lastBaseID) };

	EditFLFile(nullptr, nullptr, &linesToReplace, path);
}

// Get path to dock save file for this character.
string GetSavePath(wstring& charname, bool isCarrier)
{
	wstring shipFileName;
	HkGetCharFileName(charname, shipFileName);

	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	char tpath[1024];
	if (isCarrier)
		sprintf(tpath, R"(%s\Accts\MultiPlayer\docking_module\carriers\carrier_%ws.ini)", datapath, shipFileName.c_str());
	else
		sprintf(tpath, R"(%s\Accts\MultiPlayer\docking_module\dockedships\docked_%ws.ini)", datapath, shipFileName.c_str());

	return tpath;
}

// Overloaded function to get path directly from shipFileName.
string GetSavePath(string& shipFileName, bool isCarrier)
{
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	char tpath[1024];
	if (isCarrier)
		sprintf(tpath, R"(%s\Accts\MultiPlayer\docking_module\carriers\carrier_%ws.ini)", datapath, stows(shipFileName).c_str());
	else
		sprintf(tpath, R"(%s\Accts\MultiPlayer\docking_module\dockedships\docked_%ws.ini)", datapath, stows(shipFileName).c_str());

	return tpath;
}

// Get path to original FL account for this charname.
string GetFLAccPath(wstring& charname)
{
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	wstring wscDir;
	wstring wscCharfile;
	CAccount *acc = HkGetAccountByCharname(charname);

	HkGetAccountDirName(acc, wscDir);
	HkGetCharFileName(charname, wscCharfile);

	string path = (string(datapath) + "\\Accts\\MultiPlayer\\" + wstos(wscDir) + "\\" + wstos(wscCharfile) + ".fl");

	return path;
}

// Removes all specifil lines from file, adds specific lines to end of file, or replaces one line with few another if need to insert lines into specific position.
bool EditFLFile(vector<string> *linesToDelete, vector<string> *linesToAdd, map<string,vector<string>> *linesToReplace, string& path, bool createNew, bool compareHard)
{
	// Check if file exists
	bool foundFile = CheckIfExists(path);

	vector<string> newLines;

	// If this is new file - no need to check for lines to remove
	if (createNew == false)
	{
		fstream file;
		string line;

		file.open(path, ios::in);

		// Copy all lines to vector, except for specific line.
		if (file.is_open())
		{
			while (getline(file, line))
			{
				bool addline = true;

				if (linesToDelete != nullptr)
				{
					for_each((*linesToDelete).begin(), (*linesToDelete).end(), [&addline, &line, &newLines, &compareHard](string& str)
					{
						if (compareHard == false)
						{
							if (line.find(str) == 0)
								addline = false;
						}
						else
						{
							if (line == str)
							{
								addline = false;
							}
						}
					});
				}
				if (linesToReplace != nullptr)
				{
					for_each((*linesToReplace).begin(), (*linesToReplace).end(), [&addline, &line, &newLines](pair<const string, vector<string>>& pair)
					{
						if (line.find(pair.first) == 0)
						{
							addline = false;

							if (!pair.second.empty())
							{
								newLines.insert(newLines.end(), pair.second.begin(), pair.second.end());
							}
						}
					});
				}

				if (addline)
					newLines.push_back(line);
				else
					addline = true;
			}

			file.close();

			// Remove old file
			if(foundFile)
				_unlink(path.c_str());
		}
	}

	// Add new lines if argument is not equal to nullptr.
	if (linesToAdd != nullptr)
		newLines.insert(newLines.end(), (*linesToAdd).begin(), (*linesToAdd).end());

	if (!newLines.empty())
	{
		// Create new file and add all copied lines to it.
		FILE *newfile = fopen(path.c_str(), "w");
		if (newfile)
		{
			for (vector<string>::iterator it = newLines.begin(); it != newLines.end(); ++it)
			{
				fprintf(newfile, (*it + "\n").c_str());
			}
		}

		fclose(newfile);
	}

	return foundFile;
}

// Good function to read specific values from FL save file. Gets them in raw string format. Returns more than 1 value associated with field, if there are more than 1 field with specific name.
bool ReadFLFile(map<string, vector<string>> &fields, string& path)
{
	bool foundFile = CheckIfExists(path);

	fstream file;
	string line;

	file.open(path, ios::in);
	if (file.is_open())
	{
		if(!fields.empty())
		while (getline(file, line))
		{
			for (map<string,vector<string>>::iterator it = fields.begin(); it != fields.end(); it++)
			{
				if (line.find(it->first) == 0)
				{
					bool correct = false;
					line.replace(0, (it->first).size(), "");

					if (line.find(" ") == 0)
					{
						correct = true;
						line.erase(line.begin());
					}
					if (line.find("=") == 0)
					{
						correct = true;
						line.erase(line.begin());
					}
					if (line.find(" ") == 0)
					{
						correct = true;
						line.erase(line.begin());
					}
					if (correct)
					{
						fields[it->first].push_back(line);
					}
				}
			}
		}

		file.close();
	}

	return foundFile;
}

wstring HkGetCharnameFromCharFile(string charFile, CAccount* acc)
{
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	wstring wscDir;
	HkGetAccountDirName(acc, wscDir);

	string path = (string(datapath) + "\\Accts\\MultiPlayer\\" + wstos(wscDir) + "\\" + charFile);

	map<string, vector<string>> fields;
	fields["name"].push_back("=");
	ReadFLFile(fields, path);

	string charArray = fields["name"][1];
	wstring charName;

	for (uint i = 0; i < fields["name"][1].size() / 4; i++)
	{
		charName.push_back((wchar_t)strtol(charArray.substr(i * 4, 4).c_str(), NULL, 16));
	}

	return charName;
}