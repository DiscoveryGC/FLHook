namespace HookExt
{
	IMPORT wstring IniGetWS(uint client, const string &name);
	IMPORT string IniGetS(uint client, const string &name);
	IMPORT uint IniGetI(uint client, const string &name);
	IMPORT bool IniGetB(uint client, const string &name);
	IMPORT float IniGetF(uint client, const string &name);

	IMPORT void IniSetWS(uint client, const string &name, const wstring &value);
	IMPORT void IniSetS(uint client, const string &name, const string &value);
	IMPORT void IniSetI(uint client, const string &name, uint value);
	IMPORT void IniSetB(uint client, const string &name, bool value);
	IMPORT void IniSetF(uint client, const string &name, float value);

	IMPORT void IniSetWS(const wstring &charname, const string &name, const wstring &value);
	IMPORT void IniSetS(const wstring &charname, const string &name, const string &value);
	IMPORT void IniSetI(const wstring &charname, const string &name, uint value);
	IMPORT void IniSetB(const wstring &charname, const string &name, bool value);
	IMPORT void IniSetF(const wstring &charname, const string &name, float value);

	IMPORT void AddPOBEventData(uint client, string eventid, int count);
	IMPORT void ClearPOBEventData();
	IMPORT map<uint, EVENT_PLUGIN_POB_TRANSFER> RequestPOBEventData();
	IMPORT map<uint, string> GetMiningEventObjs();
	IMPORT void AddMiningObj(uint spaceobj, string basename);
	IMPORT void ClearMiningObjData();
};
