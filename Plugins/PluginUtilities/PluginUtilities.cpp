// Common utilities for FLHook plugins
// 
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

#include <windows.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <math.h>
#include <float.h>
#include <list>
#include <FLHook.h>
#include <plugin.h>
#include <io.h>
#include <shlwapi.h>
#include <Psapi.h>
#include <FLCoreRemoteClient.h>
#include "PluginUtilities.h"

#define ADDR_RMCLIENT_LAUNCH 0x5B40
#define ADDR_RMCLIENT_CLIENT 0x43D74

// Player in-system beaming
struct LAUNCH_PACKET
{
	uint iShip;
	uint iDunno[2];
	float fRotate[4];
	float fPos[3];
};

// Return true if this player is within the specified distance of any other player.
bool IsInRange(uint iClientID, float fDistance)
{
        list<GROUP_MEMBER> lstMembers;
        HkGetGroupMembers((const wchar_t*) Players.GetActiveCharacterName(iClientID), lstMembers);

        uint iShip;
        pub::Player::GetShip(iClientID, iShip);

        Vector pos;
        Matrix rot;
        pub::SpaceObj::GetLocation(iShip, pos, rot);

        uint iSystem;
        pub::Player::GetSystem(iClientID, iSystem);

        // For all players in system...
        struct PlayerData *pPD = 0;
        while(pPD = Players.traverse_active(pPD))
        {
                // Get the this player's current system and location in the system.
                uint iClientID2 = HkGetClientIdFromPD(pPD);
                uint iSystem2 = 0;
                pub::Player::GetSystem(iClientID2, iSystem2);
                if (iSystem != iSystem2)
                        continue;

                uint iShip2;
                pub::Player::GetShip(iClientID2, iShip2);

                Vector pos2;
                Matrix rot2;
                pub::SpaceObj::GetLocation(iShip2, pos2, rot2);

                // Ignore players who are in your group.
                bool bGrouped = false;
                foreach (lstMembers, GROUP_MEMBER, gm)
                {
                        if (gm->iClientID == iClientID2)
                        {
                                bGrouped = true;
                                break;
                        }
                }
                if (bGrouped)
                        continue;

                // Is player within the specified range of the sending char.
                if (HkDistance3D(pos, pos2) < fDistance)
                        return true;
        }
        return false;
}

CAccount* HkGetAccountByClientID(uint iClientID)
{
	if (!HkIsValidClientID(iClientID))
		return 0;

	return Players.FindAccountFromClientID(iClientID);
}

// Calculate the distance between the two vectors
float HkDistance3D(Vector v1, Vector v2)
{
	float sq1 = v1.x - v2.x, sq2 = v1.y - v2.y, sq3 = v1.z - v2.z;
	return sqrt(sq1*sq1 + sq2*sq2 + sq3*sq3);
}

// Calculate the distance between the two vectors
float HkDistance3DByShip(uint iShip1, uint iShip2)
{
	Vector v1;
	Matrix m1;
	pub::SpaceObj::GetLocation(iShip1, v1, m1);
	Vector v2;
	Matrix m2;
	pub::SpaceObj::GetLocation(iShip2, v2, m2);
	float sq1 = v1.x - v2.x, sq2 = v1.y - v2.y, sq3 = v1.z - v2.z;
	return sqrt(sq1*sq1 + sq2*sq2 + sq3*sq3);
}

HK_ERROR HkAddEquip(const wstring &wscCharname, uint iGoodID, const string &scHardpoint)
{
	HK_GET_CLIENTID(iClientID, wscCharname);

	if ((iClientID == -1) || HkIsInCharSelectMenu(iClientID))
		return HKE_NO_CHAR_SELECTED;

	if (!Players[iClientID].x3F4)
	{
		ConPrint(L"1: %u %u\n", Players[iClientID].x3F4, Players[iClientID].iBaseRoomID);
		Players[iClientID].x3F4 = 1;
		Server.ReqAddItem(iGoodID, scHardpoint.c_str(), 1, 1.0f, true, iClientID);
		Players[iClientID].x3F4 = 0;
	}
	else
	{
		Server.ReqAddItem(iGoodID, scHardpoint.c_str(), 1, 1.0f, true, iClientID);
	}

	// Add to check-list which is being compared to the users equip-list when saving
	// char to fix "Ship or Equipment not sold on base" kick
	EquipDesc ed;
	ed.sID = Players[iClientID].LastEquipID;
	ed.iCount = 1;
	ed.iArchID = iGoodID;
	Players[iClientID].lShadowEquipDescList.add_equipment_item(ed, false);

	return HKE_OK;
}

typedef void(__stdcall *_FLAntiCheat)();
typedef void(__stdcall *_FLPossibleCheatingDetected)(int iReason);

// Anti cheat checking code by mc_horst
HK_ERROR HkAntiCheat(uint iClientID)
{
#define ADDR_FL_ANTICHEAT_1 0x70120
#define ADDR_FL_ANTICHEAT_2 0x6FD20
#define ADDR_FL_ANTICHEAT_3 0x6FAF0
#define ADDR_FL_ANTICHEAT_4 0x6FAA0
#define ADDR_FL_POSSIBLE_CHEATING_DETECTED 0x6F570

	_FLAntiCheat FLAntiCheat1 = (_FLAntiCheat)((char*)hModServer + ADDR_FL_ANTICHEAT_1);
	_FLAntiCheat FLAntiCheat2 = (_FLAntiCheat)((char*)hModServer + ADDR_FL_ANTICHEAT_2);
	_FLAntiCheat FLAntiCheat3 = (_FLAntiCheat)((char*)hModServer + ADDR_FL_ANTICHEAT_3);
	_FLAntiCheat FLAntiCheat4 = (_FLAntiCheat)((char*)hModServer + ADDR_FL_ANTICHEAT_4);
	_FLPossibleCheatingDetected FLPossibleCheatingDetected = (_FLPossibleCheatingDetected)((char*)hModServer + ADDR_FL_POSSIBLE_CHEATING_DETECTED);

	// check if ship in space
	uint iShip = 0;
	pub::Player::GetShip(iClientID, iShip);
	if (iShip)
		return HKE_OK;

	char *szObjPtr;
	memcpy(&szObjPtr, &Players, 4);
	szObjPtr += 0x418 * (iClientID - 1);

	char cRes;

	////////////////////////// 1
	__asm
	{
		mov ecx, [szObjPtr]
		call[FLAntiCheat1]
			mov[cRes], al
	}

	if (cRes != 0)
	{ // kick
		HkKick(ARG_CLIENTID(iClientID));
		return HKE_UNKNOWN_ERROR;
	}

	////////////////////////// 2
	__asm
	{
		mov ecx, [szObjPtr]
		call[FLAntiCheat2]
			mov[cRes], al
	}

	if (cRes != 0)
	{ // kick
		HkKick(ARG_CLIENTID(iClientID));
		return HKE_UNKNOWN_ERROR;
	}

	////////////////////////// 3
	ulong lRet;
	ulong lCompare;
	__asm
	{
		mov ecx, [szObjPtr]
		mov eax, [ecx + 0x320]
			mov[lCompare], eax
			call[FLAntiCheat3]
			mov[lRet], eax
	}

	if (lRet > lCompare)
	{ // kick
		HkKick(ARG_CLIENTID(iClientID));
		return HKE_UNKNOWN_ERROR;
	}

	////////////////////////// 4
	__asm
	{
		mov ecx, [szObjPtr]
		call[FLAntiCheat4]
			mov[cRes], al
	}

	if (cRes != 0)
	{ // kick
		HkKick(ARG_CLIENTID(iClientID));
		return HKE_UNKNOWN_ERROR;
	}

	return HKE_OK;
}

HK_ERROR HkDeleteCharacter(CAccount *acc, wstring &wscCharname)
{
	HkLockAccountAccess(acc, true);
	flstr *str = CreateWString(wscCharname.c_str());
	Players.DeleteCharacterFromName(*str);
	FreeWString(str);
	HkUnlockAccountAccess(acc);
	return HKE_OK;
}

// This function is not exported by FLHook so we include it here
HK_ERROR HkFMsgEncodeMsg(const wstring &wscMessage, char *szBuf, uint iSize, uint &iRet)
{
	XMLReader rdr;
	RenderDisplayList rdl;
	wstring wscMsg = L"<?xml version=\"1.0\" encoding=\"UTF-16\"?><RDL><PUSH/>";
	wscMsg += L"<TRA data=\"0xE6C68400\" mask=\"-1\"/><TEXT>" + XMLText(wscMessage) + L"</TEXT>";
	wscMsg += L"<PARA/><POP/></RDL>\x000A\x000A";
	if (!rdr.read_buffer(rdl, (const char*)wscMsg.c_str(), (uint)wscMsg.length() * 2))
		return HKE_WRONG_XML_SYNTAX;

	BinaryRDLWriter	rdlwrite;
	rdlwrite.write_buffer(rdl, szBuf, iSize, iRet);

	return HKE_OK;
}

HK_ERROR HkGetOnLineTime(const wstring &wscCharname, int &iSecs)
{
	wstring wscDir;
	if (!HKHKSUCCESS(HkGetAccountDirName(wscCharname, wscDir)))
		return HKE_CHAR_DOES_NOT_EXIST;

	wstring wscFile;
	HkGetCharFileName(wscCharname, wscFile);

	string scCharFile = scAcctPath + wstos(wscDir) + "\\" + wstos(wscFile) + ".fl";
	if (HkIsEncoded(scCharFile))
	{
		string scCharFileNew = scCharFile + ".ini";
		if (!flc_decode(scCharFile.c_str(), scCharFileNew.c_str()))
			return HKE_COULD_NOT_DECODE_CHARFILE;

		iSecs = (int)IniGetF(scCharFileNew, "mPlayer", "total_time_played", 0.0f);
		DeleteFile(scCharFileNew.c_str());
	}
	else
	{
		iSecs = (int)IniGetF(scCharFile, "mPlayer", "total_time_played", 0.0f);
	}

	return HKE_OK;
}

HK_ERROR HkGetRank(const wstring &wscCharname, int &iRank)
{
	HK_ERROR err;
	wstring wscRet = L"";
	if ((err = HkFLIniGet(wscCharname, L"rank", wscRet)) != HKE_OK)
		return err;
	if (wscRet.length())
		iRank = ToInt(wscRet);
	else
		iRank = 0;
	return HKE_OK;
}

HK_ERROR HKGetShipValue(const wstring &wscCharname, float &fValue)
{
	UINT iClientID = HkGetClientIdFromCharname(wscCharname);
	if (iClientID != -1 && !HkIsInCharSelectMenu(iClientID))
	{
		HkSaveChar(iClientID);
		if (!HkIsValidClientID(iClientID))
		{
			return HKE_UNKNOWN_ERROR;
		}
	}

	fValue = 0.0f;

	uint iBaseID = 0;

	list<wstring> lstCharFile;
	HK_ERROR err = HkReadCharFile(wscCharname, lstCharFile);
	if (err != HKE_OK)
		return err;

	foreach(lstCharFile, wstring, line)
	{
		wstring wscKey = Trim(line->substr(0, line->find(L"=")));
		if (wscKey == L"base" || wscKey == L"last_base")
		{
			int iFindEqual = line->find(L"=");
			if (iFindEqual == -1)
			{
				continue;
			}

			if ((iFindEqual + 1) >= (int)line->size())
			{
				continue;
			}

			iBaseID = CreateID(wstos(Trim(line->substr(iFindEqual + 1))).c_str());
			break;
		}
	}

	foreach(lstCharFile, wstring, line)
	{
		wstring wscKey = Trim(line->substr(0, line->find(L"=")));
		if (wscKey == L"cargo" || wscKey == L"equip")
		{
			int iFindEqual = line->find(L"=");
			if (iFindEqual == -1)
			{
				continue;
			}
			int iFindComma = line->find(L",", iFindEqual);
			if (iFindComma == -1)
			{
				continue;
			}
			uint iGoodID = ToUInt(Trim(line->substr(iFindEqual + 1, iFindComma)));
			uint iGoodCount = ToUInt(Trim(line->substr(iFindComma + 1, line->find(L",", iFindComma + 1))));

			float fItemValue;
			if (pub::Market::GetPrice(iBaseID, Arch2Good(iGoodID), fItemValue) == 0)
			{
				if (arch_is_combinable(iGoodID))
				{
					fValue += fItemValue * iGoodCount;
					//ConPrint(L"market %u %0.2f = %0.2f x %u\n", iGoodID, fItemValue * iGoodCount, fItemValue, iGoodCount);
				}
				else
				{
					float* fResaleFactor = (float*)((char*)hModServer + 0x8AE7C);
					fValue += fItemValue * (*fResaleFactor);
					//ConPrint(L"market %u %0.2f = %0.2f x %0.2f x 1\n", iGoodID, fItemValue  * (*fResaleFactor), fItemValue, (*fResaleFactor));
				}
			}
		}
		else if (wscKey == L"money")
		{
			int iFindEqual = line->find(L"=");
			if (iFindEqual == -1)
			{
				continue;
			}
			uint fItemValue = ToUInt(Trim(line->substr(iFindEqual + 1)));
			fValue += fItemValue;
		}
		else if (wscKey == L"ship_archetype")
		{
			uint iShipArchID = ToUInt(Trim(line->substr(line->find(L"=") + 1, line->length())));
			const GoodInfo *gi = GoodList_get()->find_by_ship_arch(iShipArchID);
			if (gi)
			{
				gi = GoodList::find_by_id(gi->iArchID);
				if (gi)
				{
					float* fResaleFactor = (float*)((char*)hModServer + 0x8AE78);
					float fItemValue = gi->fPrice * (*fResaleFactor);
					fValue += fItemValue;
					//ConPrint(L"ship %u %0.2f = %0.2f x %0.2f\n", iShipArchID, fItemValue, gi->fPrice, *fResaleFactor);				
				}
			}
		}
	}
	return HKE_OK;
}

// Dock the client immediately
HK_ERROR HkInstantDock(uint iClientID, uint iDockObj)
{
	// check if logged in
	if (iClientID == -1)
		return HKE_PLAYER_NOT_LOGGED_IN;

	uint iShip;
	pub::Player::GetShip(iClientID, iShip);
	if (!iShip)
		return HKE_PLAYER_NOT_IN_SPACE;

	uint iSystem, iSystem2;
	pub::SpaceObj::GetSystem(iShip, iSystem);
	pub::SpaceObj::GetSystem(iDockObj, iSystem2);
	if (iSystem != iSystem2)
	{
		return HKE_PLAYER_NOT_IN_SPACE;
	}

	try {
		pub::SpaceObj::InstantDock(iShip, iDockObj, 1);
	}
	catch (...) { return HKE_PLAYER_NOT_IN_SPACE; }

	return HKE_OK;
}

// Create a new character in the specified account by emulating a create character
HK_ERROR HkNewCharacter(CAccount *acc, wstring &wscCharname)
{
	HkLockAccountAccess(acc, true);
	HkUnlockAccountAccess(acc);

	INI_Reader ini;
	if (!ini.open("..\\DATA\\CHARACTERS\\newcharacter.ini", false))
		return HKE_MPNEWCHARACTERFILE_NOT_FOUND_OR_INVALID;

	// Emulate char create by logging in.
	SLoginInfo logindata;
	wcsncpy(logindata.wszAccount, HkGetAccountID(acc).c_str(), 36);
	Players.login(logindata, MAX_CLIENT_ID + 1);

	SCreateCharacterInfo newcharinfo;
	wcsncpy(newcharinfo.wszCharname, wscCharname.c_str(), 23);
	newcharinfo.wszCharname[23] = 0;

	newcharinfo.iNickName = 0;
	newcharinfo.iBase = 0;
	newcharinfo.iPackage = 0;
	newcharinfo.iPilot = 0;

	while (ini.read_header())
	{
		if (ini.is_header("Faction"))
		{
			while (ini.read_value())
			{
				if (ini.is_value("nickname"))
					newcharinfo.iNickName = CreateID(ini.get_value_string());
				else if (ini.is_value("base"))
					newcharinfo.iBase = CreateID(ini.get_value_string());
				else if (ini.is_value("package"))
					newcharinfo.iPackage = CreateID(ini.get_value_string());
				else if (ini.is_value("pilot"))
					newcharinfo.iPilot = CreateID(ini.get_value_string());
			}
			break;
		}
	}
	ini.close();

	if (newcharinfo.iNickName == 0)
		newcharinfo.iNickName = CreateID("new_player");
	if (newcharinfo.iBase == 0)
		newcharinfo.iBase = CreateID("Li01_01_Base");
	if (newcharinfo.iPackage == 0)
		newcharinfo.iPackage = CreateID("ge_fighter");
	if (newcharinfo.iPilot == 0)
		newcharinfo.iPilot = CreateID("trent");

	// Fill struct with valid data (though it isnt used it is needed)
	newcharinfo.iDunno[4] = 65536;
	newcharinfo.iDunno[5] = 65538;
	
	newcharinfo.iDunno[7] = 1058642330;
	newcharinfo.iDunno[8] = 3206125978;
	newcharinfo.iDunno[9] = 65537;
	newcharinfo.iDunno[10] = 0;
	newcharinfo.iDunno[11] = 3206125978;
	newcharinfo.iDunno[12] = 65539;
	newcharinfo.iDunno[13] = 65540;
	newcharinfo.iDunno[14] = 65536;
	newcharinfo.iDunno[15] = 65538;
	Server.CreateNewCharacter(newcharinfo, MAX_CLIENT_ID + 1);
	HkSaveChar(wscCharname);
	Players.logout(MAX_CLIENT_ID + 1);
	return HKE_OK;
}

uint HkGetClientIDFromArg(const wstring &wscArg)
{
	uint iClientID;

	if (HkResolveId(wscArg, iClientID) == HKE_OK)
		return iClientID;

	if (HkResolveShortCut(wscArg, iClientID) == HKE_OK)
		return iClientID;

	return HkGetClientIdFromCharname(wscArg);
}

// Instructs DSAce to change an IDS string
void HkChangeIDSString(uint iClientID, uint ids, const wstring &text)
{
	DSACE_CHANGE_INFOCARD_STRUCT info;
	info.iClientID = iClientID;
	info.ids = ids;
	info.text = text;
	Plugin_Communication(DSACE_CHANGE_INFOCARD, &info);
}

void HkDelayedKick(uint iClientID, uint secs)
{
	mstime kick_time = timeInMS() + (secs * 1000);
	if (!ClientInfo[iClientID].tmKickTime || ClientInfo[iClientID].tmKickTime > kick_time)
		ClientInfo[iClientID].tmKickTime = kick_time;
}

// Move the client to the specified location
void HkRelocateClient(uint iClientID, Vector vDestination, Matrix mOrientation)
{
	Quaternion qRotation = HkMatrixToQuaternion(mOrientation);

	FLPACKET_LAUNCH pLaunch;
	pLaunch.iShip = ClientInfo[iClientID].iShip;
	pLaunch.iBase = 0;
	pLaunch.iState = 0xFFFFFFFF;
	pLaunch.fRotate[0] = qRotation.w;
	pLaunch.fRotate[1] = qRotation.x;
	pLaunch.fRotate[2] = qRotation.y;
	pLaunch.fRotate[3] = qRotation.z;
	pLaunch.fPos[0] = vDestination.x;
	pLaunch.fPos[1] = vDestination.y;
	pLaunch.fPos[2] = vDestination.z;

	HookClient->Send_FLPACKET_SERVER_LAUNCH(iClientID, pLaunch);

	uint iSystem;
	pub::Player::GetSystem(iClientID, iSystem);
	pub::SpaceObj::Relocate(ClientInfo[iClientID].iShip, iSystem, vDestination, mOrientation);
}

void HkSaveChar(uint iClientID)
{
	BYTE patch[] = { (BYTE)'\x90', (BYTE)'\x90' };
	WriteProcMem((char*)hModServer + 0x7EFA8, patch, sizeof(patch));
	pub::Save(iClientID, 1);
}

// Send a temp ban request to the tempban plugin
void HkTempBan(uint iClientID, uint iDuration)
{
	// call tempban plugin
	TEMPBAN_BAN_STRUCT tempban;
	tempban.iClientID = iClientID;
	tempban.iDuration = iDuration;
	Plugin_Communication(TEMPBAN_BAN, &tempban);
}

vector<HINSTANCE> vDLLs;

void HkLoadStringDLLs()
{
        HkUnloadStringDLLs();

        HINSTANCE hDLL = LoadLibraryEx("resources.dll", NULL, LOAD_LIBRARY_AS_DATAFILE); //typically resources.dll
        if(hDLL)
                vDLLs.push_back(hDLL);

        INI_Reader ini;
        if (ini.open("freelancer.ini", false))
        {
                while (ini.read_header())
                {
                        if (ini.is_header("Resources"))
                        {
                                while (ini.read_value())
                                {
                                        if (ini.is_value("DLL"))
                                        {
                                                hDLL = LoadLibraryEx(ini.get_value_string(0), NULL, LOAD_LIBRARY_AS_DATAFILE);
                                                if (hDLL)
                                                        vDLLs.push_back(hDLL);
                                        }
                                }
                        }
                }
                ini.close();
        }
}

void HkUnloadStringDLLs()
{
        for (uint i=0; i<vDLLs.size(); i++)
                FreeLibrary(vDLLs[i]);
        vDLLs.clear();
}

wstring HkGetAccountIDByClientID(uint iClientID)
{
	if (HkIsValidClientID(iClientID))
	{
		CAccount *acc = HkGetAccountByClientID(iClientID);
		if (acc && acc->wszAccID)
			return acc->wszAccID;
	}
	return L"";
}

wstring HkGetWStringFromIDS(uint iIDS)
{
	wchar_t wszBuf[1024];
	if (LoadStringW(vDLLs[iIDS >> 16], iIDS & 0xFFFF, wszBuf, 1024))
		return wszBuf;
	return L"";
}

HMODULE GetModuleAddr(uint iAddr)
{
	HMODULE hModArr[1024];
	DWORD iArrSizeNeeded;
	HANDLE hProcess = GetCurrentProcess();
	if (EnumProcessModules(hProcess, hModArr, sizeof(hModArr), &iArrSizeNeeded))
	{
		if (iArrSizeNeeded > sizeof(hModArr))
			iArrSizeNeeded = sizeof(hModArr);
		iArrSizeNeeded /= sizeof(HMODULE);
		for (uint i = 0; i < iArrSizeNeeded; i++)
		{
			MODULEINFO mi;
			if (GetModuleInformation(hProcess, hModArr[i], &mi, sizeof(mi)))
			{
				if (((uint)mi.lpBaseOfDll) < iAddr && (uint)mi.lpBaseOfDll + (uint)mi.SizeOfImage > iAddr)
				{
					return hModArr[i];
				}
			}
		}
	}
	return 0;
}

// Convert string to integer
int ToInt(const string &scStr)
{
	return atoi(scStr.c_str());
}

// Convert string to unsigned integer
uint ToUInt(const wstring &wscStr)
{
	return wcstoul(wscStr.c_str(), 0, 10);
}

// Convert unsigned int to hex string
string itohexs(uint value)
{
	char buf[16];
	sprintf(buf, "%08X", value);
	return buf;
}

wstring Int64ToPrettyStr(INT64 iValue)
{
	wchar_t buf[1000];
	swprintf(buf, _countof(buf), L"%I64d", iValue);
	int len = wcslen(buf);

	wstring wscBuf;
	for (int i = len - 1, j = 0; i >= 0; i--, j++)
	{
		if (j == 3)
		{
			j = 0;
			wscBuf.insert(0, L".");
		}
		wscBuf.insert(0, wstring(1, buf[i]));
	}
	return wscBuf;
}

wstring IntToStr(uint iValue)
{
	wchar_t buf[1000];
	swprintf(buf, _countof(buf), L"%u", iValue);
	return	buf;
}


string HkGetPlayerSystemS(uint iClientID)
{
        uint iSystemID;
        pub::Player::GetSystem(iClientID, iSystemID);
        char szSystemname[1024] = "";
        pub::GetSystemNickname(szSystemname, sizeof(szSystemname), iSystemID);
        return szSystemname;
}

string GetParam(string scLine, char cSplitChar, uint iPos)
{
	uint i = 0, j = 0;

	string scResult = "";
	for (i = 0, j = 0; (i <= iPos) && (j < scLine.length()); j++)
	{
		if (scLine[j] == cSplitChar)
		{
			while (((j + 1) < scLine.length()) && (scLine[j + 1] == cSplitChar))
				j++; // skip "whitechar"

			i++;
			continue;
		}

		if (i == iPos)
			scResult += scLine[j];
	}

	return scResult;
}

// Remove leading and trailing spaces from the string ~FlakCommon by Motah.
string Trim(string scIn)
{
        while (scIn.length() && (scIn[0] == ' ' || scIn[0] == '	' || scIn[0] == '\n' || scIn[0] == '\r'))
        {
                scIn = scIn.substr(1);
        }
        while (scIn.length() && (scIn[scIn.length() - 1] == L' ' || scIn[scIn.length() - 1] == '	' || scIn[scIn.length() - 1] == '\n' || scIn[scIn.length() - 1] == '\r'))
        {
                scIn = scIn.substr(0, scIn.length() - 1);
        }
        return scIn;
}

// This function is similar to GetParam but instead returns everything from the parameter 
// specified by iPos to the end of wscLine. 
//
// wscLine - the string to get parameters from 
// wcSplitChar - the seperator character 
// iPos - the parameter number to start from.
wstring GetParamToEnd(const wstring &wscLine, wchar_t wcSplitChar, uint iPos)
{
	for (uint i = 0, j = 0; (i <= iPos) && (j < wscLine.length()); j++)
	{
		if (wscLine[j] == wcSplitChar)
		{
			while (((j + 1) < wscLine.length()) && (wscLine[j + 1] == wcSplitChar))
				j++; // skip "whitechar"
			i++;
			continue;
		}
		if (i == iPos)
		{
			return wscLine.substr(j);
		}
	}
	return L"";
}

// Remove leading and trailing spaces from the string ~FlakCommon by Motah.
wstring Trim(wstring wscIn)
{
	while (wscIn.length() && (wscIn[0] == L' ' || wscIn[0] == L'	' || wscIn[0] == L'\n' || wscIn[0] == L'\r'))
	{
		wscIn = wscIn.substr(1);
	}
	while (wscIn.length() && (wscIn[wscIn.length() - 1] == L' ' || wscIn[wscIn.length() - 1] == L'	' || wscIn[wscIn.length() - 1] == L'\n' || wscIn[wscIn.length() - 1] == L'\r'))
	{
		wscIn = wscIn.substr(0, wscIn.length() - 1);
	}
	return wscIn;
}

wstring VectorToSectorCoord(uint iSystemID, Vector vPos)
{
        float scale = 1.0;
        const Universe::ISystem *iSystem = Universe::get_system(iSystemID);
        if (iSystem)
                scale = iSystem->NavMapScale;

        float fGridsize = 34000.0f / scale;
        int gridRefX = (int)((vPos.x + (fGridsize * 5)) / fGridsize) - 1;
        int gridRefZ = (int)((vPos.z + (fGridsize * 5)) / fGridsize) - 1;

        wstring wscXPos = L"X";
        if (gridRefX >= 0 && gridRefX < 8)
        {
                wchar_t* gridXLabel[] = { L"A", L"B", L"C", L"D", L"E", L"F", L"G", L"H" };
                wscXPos = gridXLabel[gridRefX];
        }

        wstring wscZPos = L"X";
        if (gridRefZ >= 0 && gridRefZ < 8)
        {
                wchar_t* gridZLabel[] = { L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8" };
                wscZPos = gridZLabel[gridRefZ];
        }

        wchar_t wszCurrentLocation[100];
        _snwprintf(wszCurrentLocation, sizeof(wszCurrentLocation), L"%s-%s", wscXPos.c_str(), 
		wscZPos.c_str());

        return wszCurrentLocation;
}

// Return the ship location in nav-map coordinates
wstring GetLocation(unsigned int iClientID)
{
        uint iSystemID = 0;
        uint iShip = 0;
        pub::Player::GetSystem(iClientID, iSystemID);
        pub::Player::GetShip(iClientID, iShip);
        if (!iSystemID || !iShip)
        {
                PrintUserCmdText(iClientID, L"ERR Not in space");
                return false;
        }

        Vector pos;
        Matrix rot;
        pub::SpaceObj::GetLocation(iShip, pos, rot);

        return VectorToSectorCoord(iSystemID, pos);
}

// Return the current time as a string
wstring GetTimeString(bool bLocalTime)
{
	SYSTEMTIME st;
	if (bLocalTime)
		GetLocalTime(&st);
	else
		GetSystemTime(&st);

	wchar_t wszBuf[100];
	_snwprintf_s(wszBuf, sizeof(wszBuf), L"%04d-%02d-%02d %02d:%02d:%02d SMT ", st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond);
	return wszBuf;
}

// Convert an orientation matrix to a pitch/yaw/roll vector.  Based on what
// Freelancer does for the save game.
Vector MatrixToEuler(const Matrix& mat)
{
	Vector x = { mat.data[0][0], mat.data[1][0], mat.data[2][0] };
	Vector y = { mat.data[0][1], mat.data[1][1], mat.data[2][1] };
	Vector z = { mat.data[0][2], mat.data[1][2], mat.data[2][2] };

	Vector vec;
	float h = (float)_hypot(x.x, x.y);
	if (h > 1 / 524288.0f)
	{
		vec.x = degrees(atan2f(y.z, z.z));
		vec.y = degrees(atan2f(-x.z, h));
		vec.z = degrees(atan2f(x.y, x.x));
	}
	else
	{
		vec.x = degrees(atan2f(-z.y, y.y));
		vec.y = degrees(atan2f(-x.z, h));
		vec.z = 0;
	}
	return vec;
}

Quaternion HkMatrixToQuaternion(Matrix m)
{
        Quaternion quaternion;
        quaternion.w = sqrt(max(0, 1 + m.data[0][0] + m.data[1][1] + m.data[2][2])) / 2;
        quaternion.x = sqrt(max(0, 1 + m.data[0][0] - m.data[1][1] - m.data[2][2])) / 2;
        quaternion.y = sqrt(max(0, 1 - m.data[0][0] + m.data[1][1] - m.data[2][2])) / 2;
        quaternion.z = sqrt(max(0, 1 - m.data[0][0] - m.data[1][1] + m.data[2][2])) / 2;
        quaternion.x = (float)_copysign(quaternion.x, m.data[2][1] - m.data[1][2]);
        quaternion.y = (float)_copysign(quaternion.y, m.data[0][2] - m.data[2][0]);
        quaternion.z = (float)_copysign(quaternion.z, m.data[1][0] - m.data[0][1]);
        return quaternion;
}

// Format a chat string in accordance with the receiver's preferences and send it. Will
// check that the receiver accepts messages from wscSender and refuses to send if necessary.
void FormatSendChat(uint iToClientID, const wstring &wscSender, const wstring &wscText, 
		const wstring &wscTextColor)
{
	if (set_bUserCmdIgnore)
	{
		foreach(ClientInfo[iToClientID].lstIgnore, IGNORE_INFO, it)
		{
			if (!HAS_FLAG(*it, L"i") && !(ToLower(wscSender).compare(ToLower((*it).wscCharname))))
				return; // ignored
			else if (HAS_FLAG(*it, L"i") && (ToLower(wscSender).find(ToLower((*it).wscCharname)) != -1))
				return; // ignored
		}
	}

	uchar cFormat;
	// adjust chatsize
	switch (ClientInfo[iToClientID].chatSize)
	{
		case CS_SMALL: cFormat = 0x90; break;
		case CS_BIG: cFormat = 0x10; break;
		default: cFormat = 0x00; break;
	}

	// adjust chatstyle
	switch (ClientInfo[iToClientID].chatStyle)
	{
		case CST_BOLD: cFormat += 0x01; break;
		case CST_ITALIC: cFormat += 0x02; break;
		case CST_UNDERLINE: cFormat += 0x04; break;
		default: cFormat += 0x00; break;
	}

	wchar_t wszFormatBuf[8];
	swprintf(wszFormatBuf, _countof(wszFormatBuf), L"%02X", (long)cFormat);
	wstring wscTRADataFormat = wszFormatBuf;
	const wstring wscTRADataSenderColor = L"FFFFFF"; // white

	wstring wscXML = L"<TRA data=\"0x" + wscTRADataSenderColor + wscTRADataFormat + 
		L"\" mask=\"-1\"/><TEXT>" + XMLText(wscSender) + L": </TEXT>" +
		L"<TRA data=\"0x" + wscTextColor + wscTRADataFormat + 
		L"\" mask=\"-1\"/><TEXT>" + XMLText(wscText) + L"</TEXT>";

	HkFMsg(iToClientID, wscXML);
}

void ini_get_wstring(INI_Reader &ini, wstring &wscValue)
{
	string scValue = ini.get_value_string();
	wscValue = L"";
	long lHiByte;
	long lLoByte;
	while (sscanf(scValue.c_str(), "%02X%02X", &lHiByte, &lLoByte) == 2)
	{
		scValue = scValue.substr(4);
		wchar_t wChar = (wchar_t)((lHiByte << 8) | lLoByte);
		wscValue.append(1, wChar);
	}
}

void ini_write_wstring(FILE *file, const string &parmname, wstring &in)
{
	fprintf(file, "%s=", parmname.c_str());
	for (int i = 0; i < (int)in.size(); i++)
	{
		UINT v1 = in[i] >> 8;
		UINT v2 = in[i] & 0xFF;
		fprintf(file, "%02x%02x", v1, v2);
	}
	fprintf(file, "\n");
}

// Print message to all ships within the specific number of meters of the player.
void PrintLocalUserCmdText(uint iClientID, const wstring &wscMsg, float fDistance)
{
        uint iShip;
        pub::Player::GetShip(iClientID, iShip);

        Vector pos;
        Matrix rot;
        pub::SpaceObj::GetLocation(iShip, pos, rot);

        uint iSystem;
        pub::Player::GetSystem(iClientID, iSystem);

        // For all players in system...
        struct PlayerData *pPD = 0;
        while(pPD = Players.traverse_active(pPD))
        {
                // Get the this player's current system and location in the system.
                uint iClientID2 = HkGetClientIdFromPD(pPD);
                uint iSystem2 = 0;
                pub::Player::GetSystem(iClientID2, iSystem2);
                if (iSystem != iSystem2)
                        continue;

                uint iShip2;
                pub::Player::GetShip(iClientID2, iShip2);

                Vector pos2;
                Matrix rot2;
                pub::SpaceObj::GetLocation(iShip2, pos2, rot2);

                // Is player within the specified range of the sending char.
                if (HkDistance3D(pos, pos2) > fDistance)
                        continue;

                PrintUserCmdText(iClientID2, L"%s", wscMsg.c_str());
        }
}

// Send a player to group message
void SendGroupChat(uint iFromClientID, const wstring &wscText)
{
	const wchar_t *wscSender = (const wchar_t*) Players.GetActiveCharacterName(iFromClientID);

	// Format and send the message a player in this group.
	list<GROUP_MEMBER> lstMembers;
	HkGetGroupMembers(wscSender, lstMembers);
	foreach(lstMembers, GROUP_MEMBER, g)
	{
		FormatSendChat(g->iClientID, wscSender, wscText, L"FF7BFF");
	}
}

void Rotate180(Matrix &rot)
{
	rot.data[0][0] = -rot.data[0][0];
	rot.data[1][0] = -rot.data[1][0];
	rot.data[2][0] = -rot.data[2][0];
	rot.data[0][2] = -rot.data[0][2];
	rot.data[1][2] = -rot.data[1][2];
	rot.data[2][2] = -rot.data[2][2];
}

void TranslateX(Vector &pos, Matrix &rot, float x)
{
	pos.x += x * rot.data[0][2];
	pos.y += x * rot.data[1][2];
	pos.z += x * rot.data[2][2];
}

void TranslateY(Vector &pos, Matrix &rot, float y)
{
	pos.x += y * rot.data[0][0];
	pos.y += y * rot.data[1][0];
	pos.z += y * rot.data[2][0];
}

void TranslateZ(Vector &pos, Matrix &rot, float z)
{
	pos.x += z * rot.data[0][1];
	pos.y += z * rot.data[1][1];
	pos.z += z * rot.data[2][1];
}

// HTML-encodes a string and returns the encoded string.
wstring HtmlEncode(wstring text)
{
	wstring sb;
	int len = text.size();
	for (int i = 0; i < len; i++)
	{
		switch (text[i])
		{
		case L'<':
			sb.append(L"&lt;");
			break;
		case L'>':
			sb.append(L"&gt;");
			break;
		case L'"':
			sb.append(L"&quot;");
			break;
		case L'&':
			sb.append(L"&amp;");
			break;
		default:
			if (text[i] > 159)
			{
				sb.append(L"&#");
				sb.append(stows(itos(static_cast<int>(text[i]))));
				sb.append(L";");
			}
			else
			{
				sb.append(1, text[i]);
			}
			break;
		}
	}
	return sb;
}

// Use this function to get the ticks since system startup. The FLHook timeInMS()
// function seems to report inaccurate time when the FLServer.exe process freezes
// (which happens due to other bugs). Used by playercntl.
mstime GetTimeInMS()
{
        static mstime msBaseTime = 0;
        static mstime msLastTickCount = 0;

        mstime msCurTime = GetTickCount();
        // GetTickCount is 32 bits and so wraps around ever 49.5 days
        // If a wrap around has occurred then 
        if (msCurTime < msLastTickCount)
                msBaseTime += (2^32);
        msLastTickCount = msCurTime;
        return msBaseTime + msLastTickCount;
}

#define PI 3.14159265f

// Convert radians to degrees.
float degrees(float rad)
{
	rad *= 180 / PI;

	// Prevent displaying -0 and prefer 180 to -180.
	if (rad < 0)
	{
		if (rad > -0.005f)
			rad = 0;
		else if (rad <= -179.995f)
			rad = 180;
	}

	// Round to two decimal places here, so %g can display it without decimals.
	float frac = modff(rad * 100, &rad);
	if (frac >= 0.5f)
		++rad;
	else if (frac <= -0.5f)
		--rad;

	return rad / 100;
}

// Return the CEqObj from the IObjRW
__declspec(naked) CEqObj * __stdcall HkGetEqObjFromObjRW(struct IObjRW *objRW)
{
   __asm
   {
      push ecx
      push edx
      mov ecx, [esp+12]
      mov edx, [ecx]
      call dword ptr[edx+0x150]
      pop edx
      pop ecx
      ret 4
   }
}

__declspec(naked) void __stdcall HkLightFuse(IObjRW *ship, uint iFuseID, float fDelay, float fLifetime, float fSkip)
{
        __asm
        {
                lea eax, [esp+8] //iFuseID
                push [esp+20] //fSkip
                push [esp+16] //fDelay
                push 0 //SUBOBJ_ID_NONE
                push eax
                push [esp+32] //fLifetime
                mov ecx, [esp+24]
                mov eax, [ecx]
                call [eax+0x1E4]
                ret 20
        }
}

__declspec(naked) void __stdcall HkUnLightFuse(IObjRW *ship, uint iFuseID, float fDunno)
{
        __asm
        {
                mov ecx, [esp+4]
                lea eax, [esp+8] //iFuseID
                push [esp+12] //fDunno
                push 0 //SUBOBJ_ID_NONE
                push eax //iFuseID
                mov eax, [ecx]
                call [eax+0x1E8]
                ret 12
        }
}
