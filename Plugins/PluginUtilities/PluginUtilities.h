// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

#pragma once

#ifndef __PluginUtilities_H__
#define __PluginUtilities_H__ 1

#define HAS_FLAG(a, b) ((a).wscFlags.find(b) != -1)

#include "DynPacket.h"

size_t WstrInsensitiveFind(wstring haystack, wstring needle);
bool IsInRange(uint iClientID, float fDistance);

CAccount* HkGetAccountByClientID(uint iClientID);

float HkDistance3D(Vector v1, Vector v2);
float HkDistance3DByShip(uint iShip1, uint iShip2);

bool HkSetEquip(uint iClientID, const list<EquipDesc>& equip);
HK_ERROR HkAddEquip(const wstring &wscCharname, uint iGoodID, const string &scHardpoint);
HK_ERROR HkAntiCheat(uint iClientID);
HK_ERROR HkDeleteCharacter(CAccount *acc, wstring &wscCharname);
HK_ERROR HkFMsgEncodeMsg(const wstring &wscMessage, char *szBuf, uint iSize, uint &iRet);
HK_ERROR HkGetOnLineTime(const wstring &wscCharname, int &iSecs);
HK_ERROR HkGetRank(const wstring &wscCharname, int &iRank);
HK_ERROR HKGetShipValue(const wstring &wscCharname, float &fValue);
HK_ERROR HkInstantDock(uint iClientID, uint iDockObj);
HK_ERROR HkNewCharacter(CAccount *acc, wstring &wscCharname);

uint HkGetClientIDFromArg(const wstring &wscArg);

void HkChangeIDSString(uint iClientID, uint ids, const wstring &text);
void HkDelayedKick(uint iClientID, uint secs);
void HkRelocateClient(uint iClientID, Vector vDestination, Matrix mOrientation);
void HkSaveChar(uint iClientID);
void HkTempBan(uint iClientID, uint iDuration);

void HkLoadStringDLLs();
void HkUnloadStringDLLs();

wstring HkGetAccountIDByClientID(uint iClientID);
wstring HkGetWStringFromIDS(uint iIDS);

HMODULE GetModuleAddr(uint iAddr);

int ToInt(const string &scStr);
uint ToUInt(const wstring &wscStr);

string itohexs(uint value);
string HkGetPlayerSystemS(uint iClientID);
string GetParam(string scLine, char cSplitChar, uint iPos);
string Trim(string scIn);

wstring GetParamToEnd(const wstring &wscLine, wchar_t wcSplitChar, uint iPos);
wstring Trim(wstring wscIn);
wstring VectorToSectorCoord(uint iSystemID, Vector vPos);
wstring GetLocation(unsigned int iClientID);
wstring GetTimeString(bool bLocalTime);

Vector MatrixToEuler(const Matrix& mat);
Quaternion HkMatrixToQuaternion(Matrix m);
Matrix TransposeMatrix(Matrix& m);
float MatrixDeterminant(Matrix& m, uint row, uint col);
Matrix MatrixDeterminateTable(Matrix& m);
void MatrixCofactorsTable(Matrix& m);
void MultiplyMatrix(Matrix& m, float num);
Matrix InverseMatrix(Matrix& m1);
Vector VectorMatrixMultiply(Vector& v1, Matrix& m1);
Vector NormalizeVector(Vector& v);

void FormatSendChat(uint iToClientID, const wstring &wscSender, const wstring &wscText, const wstring &wscTextColor);
void ini_get_wstring(INI_Reader &ini, wstring &wscValue);
void ini_write_wstring(FILE *file, const string &parmname, wstring &in);
void PrintLocalUserCmdText(uint iClientID, const wstring &wscMsg, float fDistance);
void PrintLocalMsgAroundObject(uint spaceObjId, const wstring& wscMsg, float fDistance);
void SendGroupChat(uint iFromClientID, const wstring &wscText);
void Rotate180(Matrix &rot);
void TranslateX(Vector &pos, Matrix &rot, float x);
void TranslateY(Vector &pos, Matrix &rot, float y);
void TranslateZ(Vector &pos, Matrix &rot, float z);

mstime GetTimeInMS();
float degrees(float rad);

CEqObj * __stdcall HkGetEqObjFromObjRW(struct IObjRW *objRW);

void __stdcall HkLightFuse(IObjRW *ship, uint iFuseID, float fDelay, float fLifetime, float fSkip);
void __stdcall HkUnLightFuse(IObjRW *ship, uint iFuseID, float fDelay);

#pragma pack(push, 1)
struct SETEQUIPMENT_ITEM
{
	int iCount;
	float fHealth;
	int iArchID;
	ushort sID;
	byte bMounted;
	byte bMission;
	ushort szHardPointLen;
};
#pragma pack(pop)

struct FLPACKET_SETEQUIPMENT
{
	ushort count;
	byte items[1];
};

#endif
