#include "Main.h"

void SendCommand(uint iClientID, const wstring &message)
{
	HkFMsg(iClientID, L"<TEXT>" + XMLText(message) + L"</TEXT>");
}

void SendSetBaseInfoText2(uint iClientID, const wstring &message)
{
	SendCommand(iClientID, wstring(L" SetBaseInfoText2 ") + message);
}

void SendResetMarketOverride(uint iClientID)
{
	SendCommand(iClientID, L" ResetMarketOverride");
	SendCommand(iClientID, L" SetMarketOverride 0 0 0 0");
}

inline string GetFLName(const wchar_t *key)
{
	char szName[12];

	_GetFLName __GetFLName = (_GetFLName)((char*)hModServer + 0x66370);
	__GetFLName(szName, key);

	return (string)szName;
}

// Get proxy base ID for specific system with proxy base specific to carrier.
uint GetProxyBase(uint carrierClientID, uint iSystemID)
{
	uint iBaseID;
	string systemName = wstos(HkGetSystemNickByID(iSystemID));
	string proxyBaseSuffix = Watcher.Cache[carrierClientID].dockingTraits.proxyBaseSuffix;
	string proxyBase = systemName + proxyBaseSuffix;
	pub::GetBaseID(iBaseID, proxyBase.c_str());

	return iBaseID;
}

// Gets character name from charfile and account.
wstring HkGetCharnameFromCharFile(string const &fileName, CAccount *acc)
{
	string path;
	path.reserve(dataPath.size() + 45);

	string dirName = GetFLName(acc->wszAccID);
	path = dataPath + "\\Accts\\MultiPlayer\\" + dirName + "\\" + fileName + ".fl";

	ifstream file(path);
	if (file.is_open())
	{
		string line;
		while (getline(file, line))
		{
			if (boost::algorithm::starts_with(line, "name"))
			{
				file.close();
				return EncodeWStringFromStringOfBytes(line.substr(7));
				// 7 = 4 symbols + whitespace + equation symbol + whitespace
			}
		}

		file.close();
	}

	return wstring();
}

// Get path to original FL account for this charname.
string GetFLAccPath(wstring &charname)
{
	string path;
	path.reserve(dataPath.size() + 45);

	CAccount *acc = HkGetAccountByCharname(charname);

	string dirName = GetFLName(acc->wszAccID);
	string fileName = GetFLName(charname.c_str());

	path = dataPath + "\\Accts\\MultiPlayer\\" + dirName + "\\" + fileName + ".fl";
	return path;
}

// Removes all specified lines from file which begin from one of given lines to delete.
// Replaces one line with multiple if need to insert lines into a specific position.
// Has separate functionality to remove or add hookExt data.
void EditFLFile(vector<string> &linesToDelete, map<string, vector<string>> &linesToReplace, vector<string> &hookExtLinesToAdd, vector<string> &hookExtLinesToDelete, string &path)
{
	string tempPath = path + "-temp";
	ofstream newFile(tempPath);
	ifstream file(path);

	// Copy all lines to new file with some editions.
	if (file.is_open())
	{
		string line;

	Begin:
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
					goto Begin;

			for (auto &pair : linesToReplace)
			{
				if (boost::algorithm::starts_with(line, pair.first))
				{
					for (string &replacement : pair.second)
						newFile << replacement << endl;

					goto Begin;
				}
			}

			newFile << line << endl;
		}

	hBegin:
		while (getline(file, line))
		{
			for (string &del : hookExtLinesToDelete)
				if (boost::algorithm::starts_with(line, del))
					goto hBegin;

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

// Function to find values of ini variables in file. Populates given map with values of variables that begun with a key.
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

// Expanded version of previous function. Also reads hookExt data, while previous does not.
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

	for (wchar_t &c : wstr)
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
	if (str.empty())
		return vector<string>();

	vector<string> Params;
	uint lastPos = 0;

	for (uint i = 0; i != str.size(); ++i)
	{
		if (str[i] == splitChar)
		{
			Params.push_back(str.substr(lastPos, i - lastPos));
			lastPos = i + 1;
		}
	}

	Params.push_back(str.substr(lastPos));

	return Params;
}