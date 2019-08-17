#include "Main.h"

/// Send a command to the client at destination ID 0x9999
void SendCommand(uint client, const wstring &message)
{
	HkFMsg(client, L"<TEXT>" + XMLText(message) + L"</TEXT>");
}

void SendSetBaseInfoText2(UINT client, const wstring &message)
{
	SendCommand(client, wstring(L" SetBaseInfoText2 ") + message);
}

void SendResetMarketOverride(UINT client)
{
	SendCommand(client, L" ResetMarketOverride");
	SendCommand(client, L" SetMarketOverride 0 0 0 0");
}

// Get proxy base ID for specific carrier.
uint GetProxyBaseForCarrier(uint carrierClientID)
{
	uint iBaseID;
	string systemName = wstos(HkGetSystemNickByID(Players[carrierClientID].iSystemID));
	string proxyBaseSuffix = Watcher.Cache[carrierClientID].dockingTraits.proxyBaseSuffix;
	string proxyBase = systemName + proxyBaseSuffix;
	pub::GetBaseID(iBaseID, proxyBase.c_str());

	return iBaseID;
}

// Get proxy base ID for specific system.
uint GetProxyBaseForSystem(uint carrierClientID, uint iSystemID)
{
	uint iBaseID;
	string systemName = wstos(HkGetSystemNickByID(iSystemID));
	string proxyBaseSuffix = Watcher.Cache[carrierClientID].dockingTraits.proxyBaseSuffix;
	string proxyBase = systemName + proxyBaseSuffix;
	pub::GetBaseID(iBaseID, proxyBase.c_str());

	return iBaseID;
}

// Gets character name from charfile and account.
wstring HkGetCharnameFromCharFile(string charFile, CAccount* acc)
{
	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	wstring wscDir;
	HkGetAccountDirName(acc, wscDir);

	string path = (string(datapath) + "\\Accts\\MultiPlayer\\" + wstos(wscDir) + "\\" + charFile);

	map<string, vector<string>> fields;
	fields["name"] = vector<string>();
	ReadFLFile(fields, path);

	wstring charname;
	if (!fields["name"].empty())
		charname = EncodeWStringFromStringOfBytes(fields["name"][0]);

	return charname;
}

bool CheckIfExists(string &path)
{
	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0);
}

// Get path to original FL account for this charname.
string GetFLAccPath(wstring &charname)
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

// Removes all specifil lines from file, adds specific lines to end of file, or replaces one line with few another if need to insert lines into specific position. Returns result of check if file exists.
bool EditFLFile(vector<string> *linesToDelete, vector<string> *linesToAdd, map<string, vector<string>> *linesToReplace, string &path, bool createNew, bool compareHard)
{
	bool foundFile = CheckIfExists(path);

	string tempPath = path + "-temp";
	ofstream newFile(tempPath);

	// If this is new file - no need to check for lines to remove
	if (createNew == false || foundFile == false)
	{
		ifstream file(path);

		// Copy all lines to new file, except for specific lines.
		if (file.is_open())
		{
			string line;
			while (getline(file, line))
			{
				bool addline = true;

				if (linesToDelete != nullptr)
				{
					for (vector<string>::iterator it = (*linesToDelete).begin(); it != (*linesToDelete).end(); it++)
					{
						if (compareHard == false)
						{
							if (line.find(*it) == 0)
								addline = false;
						}
						else
						{
							if (line == *it)
								addline = false;
						}
					}
				}
				if (linesToReplace != nullptr)
				{
					for (map<string, vector<string>>::iterator it = (*linesToReplace).begin(); it != (*linesToReplace).end(); it++)
					{
						if (line.find(it->first) == 0)
						{
							addline = false;

							if (!it->second.empty())
							{
								for (auto line : it->second)
									newFile << line << endl;
							}
						}
					}
				}

				if (addline)
					newFile << line << endl;
			}

			file.close();

			// Remove old file
			if (foundFile)
				_unlink(path.c_str());
		}
	}

	if (linesToAdd != nullptr)
	{
		for (auto line : *linesToAdd)
			newFile << line << endl;
	}

	newFile.close();
	remove(path.c_str());
	rename(tempPath.c_str(), path.c_str());

	return foundFile;
}

// My own function to read specific values from FL save file without ini headers. Gets them in raw string format. Returns result of check if file exists.
bool ReadFLFile(map<string, vector<string>> &fields, string &path)
{
	if (!CheckIfExists(path))
		return false;

	ifstream file(path);
	if (file.is_open())
	{
		string line;
		while (getline(file, line))
		{
			for (map<string, vector<string>>::iterator it = fields.begin(); it != fields.end(); it++)
			{
				if (line.substr(0, it->first.size()) == it->first)
				{
					uint pos = it->first.size();

					// Reject partial matches.
					if (line[pos] == ' ')
					{
						fields[it->first].push_back(line.substr(pos + 3));
						break;
					}
				}
			}
		}

		file.close();
	}

	return true;
}

// Converts text in UCS-2 FL encoding to hex string.
string DecodeWStringToStringOfBytes(wstring &wstr)
{
	string bytestr;
	bytestr.reserve(wstr.size() * 4);

	for (wchar_t c : wstr)
	{
		char buf[4];
		sprintf(buf, "%04x", c);
		bytestr += buf;
	}

	return bytestr;
}

// Converts hex string to UCS-2 text back.
wstring EncodeWStringFromStringOfBytes(string &bytestr)
{
	wstring charName;

	for (unsigned int i = 0; i < bytestr.size() / 4; i++)
	{
		charName.push_back((wchar_t)strtol(bytestr.substr(i * 4, 4).c_str(), NULL, 16));
	}

	return charName;
}

// Get vector of strings from one string by separating it by some char.
vector<string> GetParams(string &str, char splitChar)
{
	vector<string> Params;
	uint lastPos = 0;

	if (str.empty())
		return Params;

	for (uint i = 1; true; i++)
	{
		if (str[i] == splitChar)
		{
			Params.push_back(str.substr(lastPos, i - lastPos));
			lastPos = i + 1;
		}

		if (i == str.size() - 1)
		{
			Params.push_back(str.substr(lastPos));

			if (Params.empty())
				Params.push_back(str);

			break;
		}
	}

	return Params;
}

// Convert multiple strings to one by uniting them with some char.
string SetParams(vector<string> &Params, char uniteChar)
{
	string str;

	for (vector<string>::iterator it = Params.begin(); it != Params.end(); it++)
	{
		str += *it;
		if (it != Params.end() - 1)
			str += uniteChar;
	}

	return str;
}