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
	sizeof(ErrorMessage);
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
	string path;
	path.reserve(dataPath.size() + 45);

	// Get account directory name
	_GetFLName GetFLName = (_GetFLName)((char*)hModServer + 0x66370);
	char szDir[12];
	GetFLName(szDir, acc->wszAccID);

	path = dataPath + "\\Accts\\MultiPlayer\\" + szDir + "\\" + charFile + ".fl";

	map<string, vector<string>> variables;
	variables["name"] = vector<string>();
	ReadFLFile(variables, path);

	wstring charname;
	if (!variables["name"].empty())
		charname = EncodeWStringFromStringOfBytes(variables["name"][0]);

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
	string path;
	path.reserve(dataPath.size() + 45);

	char datapath[MAX_PATH];
	GetUserDataPath(datapath);

	CAccount *acc = HkGetAccountByCharname(charname);

	// Get account directory name.
	_GetFLName GetFLName = (_GetFLName)((char*)hModServer + 0x66370);
	char szDir[12];
	GetFLName(szDir, acc->wszAccID);

	// Get character file name.
	char szFile[12];
	GetFLName(szFile, charname.c_str());

	path = dataPath + "\\Accts\\MultiPlayer\\" + szDir + "\\" + szFile + ".fl";
	return path;
}

// Removes all specifil lines from file, adds specific lines to end of file, or replaces one line with few another if need to insert lines into specific position. Returns result of check if file exists.
void EditFLFile(vector<string> &linesToDelete, map<string, vector<string>> &linesToReplace, vector<string> &hookExtLinesToAdd, vector<string> &hookExtLinesToDelete, string &path)
{
	string tempPath = path + "-temp";
	ofstream newFile(tempPath);
	ifstream file(path);

	// Copy all lines to new file with some editions.
	if (file.is_open())
	{
		string line;

	begin:
		while (getline(file, line))
		{
			// Do not scan for hookext data.
			if (boost::algorithm::starts_with(line, "[flhook]"))
			{
				newFile << line << endl;
				break;
			}

			for (string &del : linesToDelete)
				if (boost::algorithm::starts_with(line, del))
					goto begin;
			for (auto &pair : linesToReplace)
			{
				if (boost::algorithm::starts_with(line, pair.first))
				{
					for (string &replacement : pair.second)
						newFile << replacement << endl;

					goto begin;
				}
			}

			newFile << line << endl;
		}

	hbegin:
		while (getline(file, line))
		{
			for (string &del : hookExtLinesToDelete)
				if (boost::algorithm::starts_with(line, del))
					goto hbegin;

			newFile << line << endl;
		}

		file.close();

		// Add hookext data.
		for (string &add : hookExtLinesToAdd)
			newFile << add << endl;

		newFile.close();
		remove(path.c_str());
		rename(tempPath.c_str(), path.c_str());
	}
}

// My own function to read specific values from FL save file without ini headers. Gets them in raw string format. Returns result of check if file exists.
void ReadFLFile(map<string, vector<string>> &variables, string &path)
{
	ifstream file(path);
	if (file.is_open())
	{
		string line;
		while (getline(file, line))
		{
			for (map<string, vector<string>>::iterator it = variables.begin(); it != variables.end(); ++it)
			{
				if (boost::algorithm::starts_with(line, it->first))
				{
					uint pos = it->first.size();

					// Reject partial matches.
					if (line[pos] == ' ')
					{
						// pos + 3 - we skip whitespace, equation symbol and whitespace.
						it->second.push_back(line.substr(pos + 3));
						break;
					}
				}
			}
		}

		file.close();
	}
}

// Expanded version of previous function.
void ReadFLFile(map<string, vector<string>> &variables, map<string, string> &hookExtData, string &path)
{
	ifstream file(path);
	if (file.is_open())
	{
		string line;
		while (getline(file, line))
		{
			if (boost::algorithm::starts_with(line, "[flhook]"))
				goto HookExtCheck;

			for (map<string, vector<string>>::iterator it = variables.begin(); it != variables.end(); ++it)
			{
				if (boost::algorithm::starts_with(line, it->first))
				{
					uint pos = it->first.size();

					// Reject partial matches.
					if (line[pos] == ' ')
					{
						// pos + 3 - we skip whitespace, equation symbol and whitespace.
						it->second.push_back(line.substr(pos + 3));
						break;
					}
				}
			}
		}

		// No lines to read remain, avoid HookExtCheck and go directly to exit procedure.
		goto Exit;

	HookExtCheck:
		while (getline(file, line))
		{
			uint pos = line.find(" ");
			hookExtData.insert(make_pair(line.substr(0, pos), line.substr(pos + 3)));
		}

	Exit:
		file.close();
	}
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

	for (uint i = 0; i < bytestr.size() / 4; ++i)
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

	for (uint i = 1; true; ++i)
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

	for (vector<string>::iterator it = Params.begin(); it != Params.end(); ++it)
	{
		str += *it;
		if (it != Params.end() - 1)
			str += uniteChar;
	}

	return str;
}