#include "hook.h"


#define ISERVER_LOG() if(set_bDebug) AddDebugLog(__FUNCSIG__);
#define ISERVER_LOGARG_WS(a) if(set_bDebug) AddDebugLog("     " #a ": %s", wstos((const wchar_t*)a).c_str());
#define ISERVER_LOGARG_S(a) if(set_bDebug) AddDebugLog("     " #a ": %s", (const char*)a);
#define ISERVER_LOGARG_UI(a) if(set_bDebug) AddDebugLog("     " #a ": %u", (uint)a);
#define ISERVER_LOGARG_I(a) if(set_bDebug) AddDebugLog("     " #a ": %d", (int)a);
#define ISERVER_LOGARG_H(a) if(set_bDebug) AddDebugLog("     " #a ": 0x%08X", (int)a);
#define ISERVER_LOGARG_F(a) if(set_bDebug) AddDebugLog("     " #a ": %f", (float)a);
#define ISERVER_LOGARG_V(a) if(set_bDebug) AddDebugLog("     " #a ": %f %f %f", (float)a.x, (float)a.y, (float)a.z);

#define CALL_CLIENT_METHOD(Method) \
	void* vRet; \
	char *tmp; \
	memcpy(&tmp, &Client, 4); \
	memcpy(&Client, &OldClient, 4); \
	HookClient->Method; \
	__asm { mov [vRet], eax } \
	memcpy(&Client, &tmp, 4); \


/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::CDPClientProxy__Disconnect(uint iClientID)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(CDPClientProxy__Disconnect(iClientID));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

double HkIClientImpl::CDPClientProxy__GetLinkSaturation(uint iClientID)
{
	// ISERVER_LOG();
	// ISERVER_LOGARG_UI(iClientID);

	char *tmp;
	memcpy(&tmp, &Client, 4);
	memcpy(&Client, &OldClient, 4);
	double dRet = HookClient->CDPClientProxy__GetLinkSaturation(iClientID);
	memcpy(&Client, &tmp, 4);

	return dRet;
}

/**************************************************************************************************************
**************************************************************************************************************/

uint HkIClientImpl::CDPClientProxy__GetSendQBytes(uint iClientID)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(CDPClientProxy__GetSendQBytes(iClientID));
	return reinterpret_cast<uint>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

uint HkIClientImpl::CDPClientProxy__GetSendQSize(uint iClientID)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(CDPClientProxy__GetSendQSize(iClientID));
	return reinterpret_cast<uint>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::nullsub(uint)
{
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_COMMON_ACTIVATECRUISE(uint iClientID, XActivateCruise& aq)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_COMMON_ACTIVATECRUISE, bool, __stdcall, (uint, XActivateCruise&), (iClientID, aq));

	CALL_CLIENT_METHOD(Send_FLPACKET_COMMON_ACTIVATECRUISE(iClientID, aq));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_COMMON_ACTIVATEEQUIP(unsigned int iClientID, struct XActivateEquip &aq)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_COMMON_ACTIVATEEQUIP, bool, __stdcall, (uint, XActivateEquip&), (iClientID, aq));

	CALL_CLIENT_METHOD(Send_FLPACKET_COMMON_ACTIVATEEQUIP(iClientID, aq));
	return reinterpret_cast<bool>(vRet);

}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_COMMON_ACTIVATETHRUSTERS(uint iClientID, XActivateThrusters& aq)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_COMMON_ACTIVATETHRUSTERS, bool, __stdcall, (uint, XActivateThrusters&), (iClientID, aq));

	CALL_CLIENT_METHOD(Send_FLPACKET_COMMON_ACTIVATETHRUSTERS(iClientID, aq));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_COMMON_FIREWEAPON(uint iClientID, FLPACKET_UNKNOWN& pDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_COMMON_FIREWEAPON, bool, __stdcall, (uint, FLPACKET_UNKNOWN&), (iClientID, pDunno));

	CALL_CLIENT_METHOD(Send_FLPACKET_COMMON_FIREWEAPON(iClientID, pDunno));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_COMMON_GOTRADELANE(uint iClientID, XGoTradelane& tl)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_COMMON_GOTRADELANE(iClientID, tl));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_COMMON_JETTISONCARGO(uint iClientID, XJettisonCargo& jc)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_COMMON_JETTISONCARGO(iClientID, jc));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_COMMON_PLAYER_TRADE(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_COMMON_PLAYER_TRADE(iClientID, iDunno));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_COMMON_REQUEST_BEST_PATH(uint iClientID, unsigned char *p2, int p3)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_COMMON_REQUEST_BEST_PATH(iClientID, p2, p3));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_COMMON_REQUEST_GROUP_POSITIONS(uint iClientID, unsigned char *p2, int p3)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_COMMON_REQUEST_GROUP_POSITIONS(iClientID, p2, p3));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_COMMON_REQUEST_PLAYER_STATS(uint iClientID, unsigned char *p2, int p3)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_COMMON_REQUEST_PLAYER_STATS(iClientID, p2, p3));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_COMMON_SET_INTERFACE_STATE(uint iClientID, unsigned char *p2, int p3)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_COMMON_SET_INTERFACE_STATE(iClientID, p2, p3));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_COMMON_SET_MISSION_LOG(uint iClientID, unsigned char *p2, int p3)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_COMMON_SET_MISSION_LOG(iClientID, p2, p3));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_COMMON_SET_VISITED_STATE(uint iClientID, unsigned char *p2, int p3)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_COMMON_SET_VISITED_STATE(iClientID, p2, p3));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_COMMON_SET_WEAPON_GROUP(uint iClientID, unsigned char *p2, int p3)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_COMMON_SET_WEAPON_GROUP(iClientID, p2, p3));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_COMMON_SETTARGET(uint iClientID, XSetTarget& st)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_COMMON_SETTARGET(iClientID, st));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_COMMON_STOPTRADELANE(uint iClientID, uint iShip, uint iArchTradelane1, uint iArchTradelane2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_COMMON_STOPTRADELANE(iClientID, iShip, iArchTradelane1, iArchTradelane2));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_COMMON_UPDATEOBJECT(uint iClientID, SSPObjUpdateInfo& pUpdate)
{
	//ISERVER_LOG();
	//ISERVER_LOGARG_UI(iClientID);

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_COMMON_UPDATEOBJECT, bool, __stdcall, (uint, SSPObjUpdateInfo&), (iClientID, pUpdate));

	CALL_CLIENT_METHOD(Send_FLPACKET_COMMON_UPDATEOBJECT(iClientID, pUpdate));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_ACTIVATEOBJECT(uint iClientID, XActivateEquip& aq)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_ACTIVATEOBJECT, bool, __stdcall, (uint, XActivateEquip&), (iClientID, aq));

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_ACTIVATEOBJECT(iClientID, aq));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_BURNFUSE(uint iClientID, FLPACKET_BURNFUSE& pBurnfuse)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_BURNFUSE(iClientID, pBurnfuse));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_CHARACTERINFO(uint iClientID, FLPACKET_UNKNOWN& pDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	ClientInfo[iClientID].iCharMenuEnterTime = time(0);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_CHARACTERINFO(iClientID, pDunno));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_CHARSELECTVERIFIED(uint iClientID, FLPACKET_UNKNOWN& pDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_CHARSELECTVERIFIED(iClientID, pDunno));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_CREATECOUNTER(uint iClientID, FLPACKET_UNKNOWN& pDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_CREATECOUNTER(iClientID, pDunno));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_CREATEGUIDED(uint& iClientID, FLPACKET_CREATEGUIDED& createGuidedPacket)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_CREATEGUIDED, bool, __stdcall, (uint&, FLPACKET_CREATEGUIDED&), (iClientID, createGuidedPacket));

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_CREATEGUIDED(iClientID, createGuidedPacket));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_CREATELOOT(uint iClientID, FLPACKET_UNKNOWN& pDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_CREATELOOT, bool, __stdcall, (uint, FLPACKET_UNKNOWN&), (iClientID, pDunno));

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_CREATELOOT(iClientID, pDunno));

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_CREATELOOT_AFTER, bool, __stdcall, (uint, FLPACKET_UNKNOWN&), (iClientID, pDunno));

	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_CREATEMINE(uint iClientID, FLPACKET_UNKNOWN& pDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_CREATEMINE(iClientID, pDunno));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/


bool HkIClientImpl::Send_FLPACKET_SERVER_CREATESHIP(uint iClientID, FLPACKET_CREATESHIP& pShip)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_CREATESHIP, bool, __stdcall, (uint, FLPACKET_CREATESHIP&), (iClientID, pShip));

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_CREATESHIP(iClientID, pShip));

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_CREATESHIP_AFTER, bool, , (uint, FLPACKET_CREATESHIP&), (iClientID, pShip));

	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_CREATESOLAR(uint iClientID, FLPACKET_CREATESOLAR& pSolar)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_CREATESOLAR, bool, __stdcall, (uint, FLPACKET_CREATESOLAR&), (iClientID, pSolar));

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_CREATESOLAR(iClientID, pSolar));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_DAMAGEOBJECT(uint iClientID, uint iObj, DamageList& dmlist)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);
	ISERVER_LOGARG_UI(iObj);
	//ISERVER_LOGARG_UI(damageentries);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_DAMAGEOBJECT(iClientID, iObj, dmlist));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_DESTROYOBJECT(uint iClientID, FLPACKET_DESTROYOBJECT& pDestroy)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_DESTROYOBJECT, bool, __stdcall, (uint, FLPACKET_DESTROYOBJECT&), (iClientID, pDestroy));

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_DESTROYOBJECT(iClientID, pDestroy));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_FORMATION_UPDATE(uint iClientID, uint iShipID, Vector& vFormationOffset)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_FORMATION_UPDATE(iClientID, iShipID, vFormationOffset));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_GFCOMPLETEAMBIENTSCRIPTLIST(uint iClientID, uint iRoom)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);
	ISERVER_LOGARG_UI(iRoom);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_GFCOMPLETEAMBIENTSCRIPTLIST(iClientID, iRoom));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_GFCOMPLETECHARLIST(uint iClientID, uint iRoom)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);
	ISERVER_LOGARG_UI(iRoom);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_GFCOMPLETECHARLIST(iClientID, iRoom));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_GFCOMPLETEMISSIONCOMPUTERLIST(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_GFCOMPLETEMISSIONCOMPUTERLIST(iClientID, iDunno));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_GFCOMPLETENEWSBROADCASTLIST(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_GFCOMPLETENEWSBROADCASTLIST(iClientID, iDunno));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_GFCOMPLETESCRIPTBEHAVIORLIST(uint iClientID, uint iRoom)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);
	ISERVER_LOGARG_UI(iRoom);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_GFCOMPLETESCRIPTBEHAVIORLIST(iClientID, iRoom));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_GFDESTROYMISSIONCOMPUTER(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_GFDESTROYMISSIONCOMPUTER(iClientID, iDunno, iDunno2));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_GFDESTROYSCRIPTBEHAVIOR(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_GFDESTROYSCRIPTBEHAVIOR(iClientID, iDunno, iDunno2));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_GFMISSIONVENDORACCEPTANCE(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_GFMISSIONVENDORACCEPTANCE(iClientID, iDunno, iDunno2));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_GFMISSIONVENDORWHYEMPTY(uint iClientID, uint iReason)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_GFMISSIONVENDORWHYEMPTY(iClientID, iReason));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_GFSCRIPTBEHAVIOR(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_GFSCRIPTBEHAVIOR(iClientID, iDunno, iDunno2));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_GFUPDATECHAR(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_GFUPDATECHAR(iClientID, iDunno, iDunno2));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_GFUPDATEMISSIONCOMPUTER(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_GFUPDATEMISSIONCOMPUTER(iClientID, iDunno, iDunno2));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_GFUPDATENEWSBROADCAST(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_GFUPDATENEWSBROADCAST(iClientID, iDunno, iDunno2));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_ITEMTRACTORED(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_ITEMTRACTORED(iClientID, iDunno));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_LAND(uint iClientID, FLPACKET_LAND& pLand)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_LAND, bool, __stdcall, (uint, FLPACKET_LAND&), (iClientID, pLand));

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_LAND(iClientID, pLand));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_LAUNCH(uint iClientID, FLPACKET_LAUNCH& pLaunch)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_LAUNCH, bool, __stdcall, (uint, FLPACKET_LAUNCH&), (iClientID, pLaunch));

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_LAUNCH(iClientID, pLaunch));
	return reinterpret_cast<bool>(vRet);
}


/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_LOGINRESPONSE(uint iClientID, FLPACKET_UNKNOWN& pDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_LOGINRESPONSE(iClientID, pDunno));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_MARKOBJ(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_MARKOBJ(iClientID, iDunno, iDunno2));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_MISCOBJUPDATE_6(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);
	ISERVER_LOGARG_UI(iDunno);
	ISERVER_LOGARG_UI(iDunno2);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_MISCOBJUPDATE_6(iClientID, iDunno, iDunno2));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_MISCOBJUPDATE_7(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);
	ISERVER_LOGARG_UI(iDunno);
	ISERVER_LOGARG_UI(iDunno2);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_MISCOBJUPDATE_7(iClientID, iDunno, iDunno2));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_MISCOBJUPDATE(uint iClientID, FLPACKET_UNKNOWN& pDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_MISCOBJUPDATE(iClientID, pDunno));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_MISCOBJUPDATE_2(uint iClientID, uint iObject, uint iFaction)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);
	// TODO: Missing a flag here?
	ISERVER_LOGARG_UI(iObject);
	ISERVER_LOGARG_UI(iFaction);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_MISCOBJUPDATE_2(iClientID, iObject, iFaction));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_MISCOBJUPDATE_3(uint iClientID, uint iTargetID, uint iRank)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);
	ISERVER_LOGARG_UI(iTargetID);
	ISERVER_LOGARG_UI(iRank);

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_MISCOBJUPDATE_3, bool, __stdcall, (uint, uint, uint), (iClientID, iTargetID, iRank));

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_MISCOBJUPDATE_3(iClientID, iTargetID, iRank));

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_MISCOBJUPDATE_3_AFTER, bool, , (uint, uint, uint), (iClientID, iTargetID, iRank));

	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_MISCOBJUPDATE_4(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_MISCOBJUPDATE_4(iClientID, iDunno, iDunno2));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_MISCOBJUPDATE_5(uint iClientID, uint iClientID2, uint iSystemID)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);
	ISERVER_LOGARG_UI(iClientID2);
	ISERVER_LOGARG_UI(iSystemID);

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_MISCOBJUPDATE_5, bool, __stdcall, (uint, uint, uint), (iClientID, iClientID2, iSystemID));

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_MISCOBJUPDATE_5(iClientID, iClientID2, iSystemID));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_OBJECTCARGOUPDATE(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_OBJECTCARGOUPDATE(iClientID, iDunno, iDunno2));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_PLAYERLIST(uint iClientID, wchar_t* wszName, uint iDunno, char szDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_PLAYERLIST(iClientID, wszName, iDunno, szDunno));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_PLAYERLIST_2(uint iClientID)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_PLAYERLIST_2(iClientID));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_REQUEST_RETURNED(uint iClientID, uint iShipID, uint iFlag, uint iDunno3, uint iDunno4)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_REQUEST_RETURNED(iClientID, iShipID, iFlag, iDunno3, iDunno4));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_REQUESTCREATESHIPRESP(uint iClientID, bool bResponse, uint iShipID)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_REQUESTCREATESHIPRESP, bool, __stdcall, (uint, bool, uint), (iClientID, bResponse, iShipID));

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_REQUESTCREATESHIPRESP(iClientID, bResponse, iShipID));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_SCANNOTIFY(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_SCANNOTIFY(iClientID, iDunno, iDunno2));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_SENDCOMM(uint iClientID, uint iDunno, uint iDunno2, uint iDunno3,
	uint iDunno4, uint iDunno5, uint iDunno6, uint iDunno7,
	uint iDunno8, uint iDunno9, uint iDunno10, uint iDunno11,
	uint iDunno12, uint iDunno13, uint iDunno14, uint iDunno15,
	uint iDunno16, uint iDunno17, uint iDunno18, uint iDunno19,
	uint iDunno20, uint iDunno21, uint iDunno22)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_SENDCOMM(iClientID, iDunno, iDunno2, iDunno3, iDunno4, iDunno5, iDunno6, iDunno7, iDunno8, iDunno9, iDunno10
		, iDunno11, iDunno12, iDunno13, iDunno14, iDunno15, iDunno16, iDunno17, iDunno18, iDunno19, iDunno20, iDunno21, iDunno22));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_SET_MISSION_MESSAGE(uint iClientID, FLPACKET_UNKNOWN& pDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_SET_MISSION_MESSAGE(iClientID, pDunno));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_SETADDITEM(uint iClientID, FLPACKET_UNKNOWN& pDunno, FLPACKET_UNKNOWN& pDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_SETADDITEM(iClientID, pDunno, pDunno2));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_SETCASH(uint iClientID, uint iCash)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_SETCASH(iClientID, iCash));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_SETCOLLISIONGROUPS(uint iClientID, st6::list<CollisionGroupDesc>& collisionGrpList)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_SETCOLLISIONGROUPS(iClientID, collisionGrpList));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_SETEQUIPMENT(uint iClientID, st6::vector<EquipDesc>& equipVec)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_SETEQUIPMENT(iClientID, equipVec));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_SETHULLSTATUS(uint iClientID, float fStatus)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_SETHULLSTATUS(iClientID, fStatus));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_SETMISSIONOBJECTIVES(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_SETMISSIONOBJECTIVES(iClientID, iDunno));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_SETREPUTATION(uint iClientID, FLPACKET_SETREPUTATION &pSetRep)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_SETREPUTATION(iClientID, pSetRep));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_SETSHIPARCH(uint iClientID, uint iShipArch)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_SETSHIPARCH, bool, , (uint, uint), (iClientID, iShipArch));

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_SETSHIPARCH(iClientID, iShipArch));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_SETSTARTROOM(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_SETSTARTROOM(iClientID, iDunno, iDunno2));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_GFDESTROYCHARACTER(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_GFDESTROYCHARACTER(iClientID, iDunno, iDunno2));
	return reinterpret_cast<bool>(vRet);
}


/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_SYSTEM_SWITCH_IN(uint iClientID, FLPACKET_SYSTEM_SWITCH_IN &switchInPacket)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_SYSTEM_SWITCH_IN(iClientID, switchInPacket));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_SYSTEM_SWITCH_OUT(uint iClientID, FLPACKET_SYSTEM_SWITCH_OUT &switchOutPacket)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_PLUGINS(PLUGIN_HkIClientImpl_Send_FLPACKET_SERVER_SYSTEM_SWITCH_OUT, bool, __stdcall, (uint, FLPACKET_SYSTEM_SWITCH_OUT&), (iClientID, switchOutPacket));

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_SYSTEM_SWITCH_OUT(iClientID, switchOutPacket));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Send_FLPACKET_SERVER_USE_ITEM(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(Send_FLPACKET_SERVER_USE_ITEM(iClientID, iDunno));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::SendPacket(void* pDunno1, void* pDunno2)
{
	CALL_CLIENT_METHOD(SendPacket(pDunno1, pDunno2));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::Shutdown()
{

	CALL_CLIENT_METHOD(Shutdown());
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::Startup(uint iDunno, uint iDunno2)
{

	// load universe / we load the universe directly before the server becomes internet accessible
	lstBases.clear();
	Universe::IBase *base = Universe::GetFirstBase();
	while (base)
	{
		BASE_INFO bi;
		bi.bDestroyed = false;
		bi.iObjectID = base->lSpaceObjID;
		char *szBaseName = "";
		__asm
		{
			pushad
			mov ecx, [base]
			mov eax, [base]
			mov eax, [eax]
			call[eax + 4]
			mov[szBaseName], eax
			popad
		}

		bi.scBasename = szBaseName;
		bi.iBaseID = CreateID(szBaseName);
		lstBases[bi.iBaseID] = bi;
		pub::System::LoadSystem(base->iSystemID);

		base = Universe::GetNextBase();
	}

	CALL_CLIENT_METHOD(Startup(iDunno, iDunno2));
	return reinterpret_cast<bool>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

bool HkIClientImpl::DispatchMsgs()
{

	/*	long lRet;
		char *tmp;
		WriteProcMem(&tmp, &Client, 4);
		WriteProcMem(&Client, &OldClient, 4);
		HookClient->DispatchMsgs();
		__asm { mov [lRet], eax }
		WriteProcMem(&Client, &tmp, 4); */

	cdpserver->DispatchMsgs(); // calls IServerImpl functions, which also call HkIClientImpl functions
	return true;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_100(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_100(iClientID, iDunno, iDunno2));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_101(uint iClientID, FLPACKET_UNKNOWN &pDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_101(iClientID, pDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_102(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_102(iClientID, iDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_103(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_103(iClientID, iDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_104(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_104(iClientID, iDunno, iDunno2));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_105(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_105(iClientID, iDunno, iDunno2));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_106(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_106(iClientID, iDunno, iDunno2));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_107(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_107(iClientID, iDunno, iDunno2));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_109(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_109(iClientID, iDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_112(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_112(iClientID, iDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_121(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_121(iClientID, iDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_123(uint iClientID, uint iDunno, uint iDunno2, uint iDunno3, uint iDunno4, uint iDunno5, uint iDunno6)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_123(iClientID, iDunno, iDunno2, iDunno3, iDunno4, iDunno5, iDunno6));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_124(uint iClientID)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_124(iClientID));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_125(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_125(iClientID, iDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

int HkIClientImpl::unknown_126(char *szUnknown)
{
	ISERVER_LOG();
	ISERVER_LOGARG_S(szUnknown);

	CALL_CLIENT_METHOD(unknown_126(szUnknown));
	return reinterpret_cast<int>(vRet);
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_26(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_26(iClientID, iDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_28(uint iClientID, uint iDunno, uint iDunno2, uint iDunno3)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_28(iClientID, iDunno, iDunno2, iDunno3));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_36(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_36(iClientID, iDunno, iDunno2));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_37(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_37(iClientID, iDunno, iDunno2));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_44(uint iClientID, uint iDunno, uint iDunno2)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_44(iClientID, iDunno, iDunno2));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_53(uint iClientID, FLPACKET_UNKNOWN &pDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_53(iClientID, pDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_54(uint iClientID, uint iDunno, uint iDunno2, uint iDunno3)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_54(iClientID, iDunno, iDunno2, iDunno3));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_6(uint iClientID, FLPACKET_UNKNOWN &pDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_6(iClientID, pDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_63(uint iClientID, FLPACKET_UNKNOWN &pDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_63(iClientID, pDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_68(uint iClientID, FLPACKET_UNKNOWN &pDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_68(iClientID, pDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_70(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_70(iClientID, iDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_72(uint iClientID, FLPACKET_UNKNOWN &pDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_72(iClientID, pDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_74(uint iClientID, FLPACKET_UNKNOWN &pDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_74(iClientID, pDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_75(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_75(iClientID, iDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_77(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_77(iClientID, iDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_79(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_79(iClientID, iDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_80(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_80(iClientID, iDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_81(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_81(iClientID, iDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_82(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_82(iClientID, iDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_83(uint iClientID, char *szDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_83(iClientID, szDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_85(uint iClientID, FLPACKET_UNKNOWN &pDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_85(iClientID, pDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_86(uint iClientID, uint iDunno, uint iDunno2, uint iDunno3)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_86(iClientID, iDunno, iDunno2, iDunno3));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_89(uint iClientID, FLPACKET_UNKNOWN &pDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_89(iClientID, pDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_90(uint iClientID)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_90(iClientID));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_91(uint iClientID, uint iDunno)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_91(iClientID, iDunno));
	return;
}

/**************************************************************************************************************
**************************************************************************************************************/

void HkIClientImpl::unknown_96(uint iClientID, uint iDunno, uint iDunno2, uint iDunno3)
{
	ISERVER_LOG();
	ISERVER_LOGARG_UI(iClientID);

	CALL_CLIENT_METHOD(unknown_96(iClientID, iDunno, iDunno2, iDunno3));
	return;
}