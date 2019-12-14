#include "hook.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HkGetPlayerIP(uint iClientID, wstring &wscIP)
{
	wscIP = L"";
	CDPClientProxy *cdpClient = g_cClientProxyArray[iClientID - 1];
	if (!cdpClient)
		return;

	// get ip
	char *szP1;
	char *szIDirectPlay8Address;
	wchar_t wszHostname[] = L"hostname";
	memcpy(&szP1, (char*)cdpSrv + 4, 4);

	wchar_t wszIP[1024] = L"";
	long lSize = sizeof(wszIP);
	long lDataType = 1;
	__asm
	{
		push 0; dwFlags
		lea edx, szIDirectPlay8Address
		push edx; pAddress
		mov edx, [cdpClient]
		mov edx, [edx + 8]
		push edx; dpnid
		mov eax, [szP1]
		push eax
		mov ecx, [eax]
		call dword ptr[ecx + 0x28]; GetClientAddress
		cmp eax, 0
		jnz some_error

		lea eax, lDataType
		push eax
		lea eax, lSize
		push eax
		lea eax, wszIP
		push eax
		lea eax, wszHostname
		push eax
		mov ecx, [szIDirectPlay8Address]
		push ecx
		mov ecx, [ecx]
		call dword ptr[ecx + 0x40]; GetComponentByName

		mov ecx, [szIDirectPlay8Address]
		push ecx
		mov ecx, [ecx]
		call dword ptr[ecx + 0x08]; Release
		some_error :
	}

	wscIP = wszIP;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

HK_ERROR HkGetPlayerInfo(const wstring &wscCharname, HKPLAYERINFO &pi, bool bAlsoCharmenu)
{
	HK_GET_CLIENTID(iClientID, wscCharname);

	if (iClientID == -1 || (HkIsInCharSelectMenu(iClientID) && !bAlsoCharmenu))
		return HKE_PLAYER_NOT_LOGGED_IN; // not on server

	const wchar_t *wszActiveCharname = (wchar_t*)Players.GetActiveCharacterName(iClientID);

	pi.iClientID = iClientID;
	pi.wscCharname = wszActiveCharname ? wszActiveCharname : L"";
	pi.wscBase = pi.wscSystem = L"";

	uint iBase = 0;
	uint iSystem = 0;
	pub::Player::GetBase(iClientID, iBase);
	pub::Player::GetSystem(iClientID, iSystem);
	pub::Player::GetShip(iClientID, pi.iShip);

	if (iBase)
	{
		char szBasename[1024] = "";
		pub::GetBaseNickname(szBasename, sizeof(szBasename), iBase);
		pi.wscBase = stows(szBasename);
	}

	if (iSystem)
	{
		char szSystemname[1024] = "";
		pub::GetSystemNickname(szSystemname, sizeof(szSystemname), iSystem);
		pi.wscSystem = stows(szSystemname);
		pi.iSystem = iSystem;
	}

	// get ping
	DPN_CONNECTION_INFO ci;
	HkGetConnectionStats(iClientID, ci);
	pi.ci = ci;

	// get ip
	HkGetPlayerIP(iClientID, pi.wscIP);

	return HKE_OK;
}

list<HKPLAYERINFO> HkGetPlayers()
{
	list<HKPLAYERINFO> lstRet;
	wstring wscRet;

	struct PlayerData *pPD = 0;
	while (pPD = Players.traverse_active(pPD))
	{
		uint iClientID = HkGetClientIdFromPD(pPD);

		if (HkIsInCharSelectMenu(iClientID))
			continue;

		HKPLAYERINFO pi;
		HkGetPlayerInfo(ARG_CLIENTID(iClientID), pi, false);
		lstRet.push_back(pi);
	}

	return lstRet;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

HK_ERROR HkGetConnectionStats(uint iClientID, DPN_CONNECTION_INFO &ci)
{
	if (iClientID < 1 || iClientID > MAX_CLIENT_ID)
		return HKE_INVALID_CLIENT_ID;

	CDPClientProxy *cdpClient = g_cClientProxyArray[iClientID - 1];

	if (!cdpClient || !cdpClient->GetConnectionStats(&ci))
		return HKE_INVALID_CLIENT_ID;

	return HKE_OK;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

HK_ERROR HkSetAdmin(const wstring &wscCharname, const wstring &wscRights)
{
	HK_GET_CLIENTID(iClientID, wscCharname);
	CAccount *acc;
	if (iClientID == -1) {
		flstr *str = CreateWString(wscCharname.c_str());
		acc = Players.FindAccountFromCharacterName(*str);
		FreeWString(str);
		if (!acc)
			return HKE_CHAR_DOES_NOT_EXIST;;
	}
	else {
		acc = Players.FindAccountFromClientID(iClientID);
	}

	wstring wscDir;
	HkGetAccountDirName(acc, wscDir);
	string scAdminFile = scAcctPath + wstos(wscDir) + "\\flhookadmin.ini";
	IniWrite(scAdminFile, "admin", "rights", wstos(wscRights));
	return HKE_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

HK_ERROR HkGetAdmin(const wstring &wscCharname, wstring &wscRights)
{
	wscRights = L"";
	HK_GET_CLIENTID(iClientID, wscCharname);
	CAccount *acc;
	if (iClientID == -1) {
		flstr *str = CreateWString(wscCharname.c_str());
		acc = Players.FindAccountFromCharacterName(*str);
		FreeWString(str);
		if (!acc)
			return HKE_CHAR_DOES_NOT_EXIST;;
	}
	else {
		acc = Players.FindAccountFromClientID(iClientID);
	}

	wstring wscDir;
	HkGetAccountDirName(acc, wscDir);
	string scAdminFile = scAcctPath + wstos(wscDir) + "\\flhookadmin.ini";

	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile(scAdminFile.c_str(), &fd);
	if (hFind == INVALID_HANDLE_VALUE)
		return HKE_PLAYER_NO_ADMIN;;

	FindClose(hFind);
	wscRights = stows(IniGetS(scAdminFile, "admin", "rights", ""));

	return HKE_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

HK_ERROR HkDelAdmin(const wstring &wscCharname)
{
	HK_GET_CLIENTID(iClientID, wscCharname);
	CAccount *acc;
	if (iClientID == -1) {
		flstr *str = CreateWString(wscCharname.c_str());
		acc = Players.FindAccountFromCharacterName(*str);
		FreeWString(str);
		if (!acc)
			return HKE_CHAR_DOES_NOT_EXIST;;
	}
	else {
		acc = Players.FindAccountFromClientID(iClientID);
	}

	wstring wscDir;
	HkGetAccountDirName(acc, wscDir);
	string scAdminFile = scAcctPath + wstos(wscDir) + "\\flhookadmin.ini";
	DeleteFile(scAdminFile.c_str());
	return HKE_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool g_bNPCDisabled = false;

HK_ERROR HkChangeNPCSpawn(bool bDisable)
{
	if (g_bNPCDisabled && bDisable)
		return HKE_OK;
	else if (!g_bNPCDisabled && !bDisable)
		return HKE_OK;

	char szJump[1];
	char szCmp[1];
	if (bDisable) {
		szJump[0] = '\xEB';
		szCmp[0] = '\xFF';
	}
	else {
		szJump[0] = '\x75';
		szCmp[0] = '\xF9';
	}

	void *pAddress = CONTENT_ADDR(ADDR_DISABLENPCSPAWNS1);
	WriteProcMem(pAddress, &szJump, 1);
	pAddress = CONTENT_ADDR(ADDR_DISABLENPCSPAWNS2);
	WriteProcMem(pAddress, &szCmp, 1);
	g_bNPCDisabled = bDisable;
	return HKE_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

HK_ERROR HkGetBaseStatus(const wstring &wscBasename, float &fHealth, float &fMaxHealth)
{
	uint iBaseID = 0;
	pub::GetBaseID(iBaseID, wstos(wscBasename).c_str());
	if (!iBaseID)
	{
		string scBaseShortcut = IniGetS(set_scCfgFile, "names", wstos(wscBasename), "");
		if (!scBaseShortcut.length())
			return HKE_INVALID_BASENAME;

		if (pub::GetBaseID(iBaseID, scBaseShortcut.c_str()) == -4)
			return HKE_INVALID_BASENAME;
	}

	Universe::IBase *base = Universe::get_base(iBaseID);
	pub::SpaceObj::GetHealth(base->lSpaceObjID, fHealth, fMaxHealth);
	return HKE_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

Fuse* HkGetFuseFromID(uint iFuseID)
{
	int iDunno;
	Fuse *fuse;
	__asm
	{
		mov edx, 0x6CFD390
		call edx

		lea ecx, iFuseID
		push ecx
		lea ecx, iDunno
		push ecx
		mov ecx, eax
		mov edx, 0x6D15D10
		call edx
		mov edx, [iDunno]
		mov edi, [edx + 0x10]
		mov fuse, edi
	}
	return fuse;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////


__declspec(naked) bool __stdcall HkLightFuse(IObjRW *ship, uint iFuseID, float fDelay, float fLifetime, float fSkip)
{
	__asm
	{
		lea eax, [esp + 8] //iFuseID
		push[esp + 20] //fSkip
		push[esp + 16] //fDelay
		push 0 //SUBOBJ_ID_NONE
		push eax
		push[esp + 32] //fLifetime
		mov ecx, [esp + 24]
		mov eax, [ecx]
		call[eax + 0x1E4]
		ret 20
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Returns true if a fuse was unlit
__declspec(naked) bool __stdcall HkUnLightFuse(IObjRW *ship, uint iFuseID, float fDunno)
{
	__asm
	{
		mov ecx, [esp + 4]
		lea eax, [esp + 8] //iFuseID
		push[esp + 12] //fDunno
		push 0 //SUBOBJ_ID_NONE
		push eax //iFuseID
		mov eax, [ecx]
		call[eax + 0x1E8]
		ret 12
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HkTest(int iArg, int iArg2, int iArg3)
{
}
