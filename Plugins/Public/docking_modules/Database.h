#pragma once

#include "boost\lexical_cast.hpp"
#include "boost\algorithm\string.hpp"
#include "boost/algorithm/string/join.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <string>
#include <map>
#include <FLHook.h>
#include <PluginUtilities.h>
#include "../hookext_plugin/hookext_exports.h"

// Utilities.cpp
string GetFLAccPath(wstring &charname);
void EditFLFile(vector<string> &linesToDelete, map<string, vector<string>> &linesToReplace, vector<string> &hookExtLinesToAdd, vector<string> &hookExtLinesToDelete, string &path);
void ReadFLFile(map<string, vector<string>> &variables, map<string, string> &hookExtData, string &path);
void ReadFLFile(map<string, vector<string>> &variables, string &path);
string DecodeWStringToStringOfBytes(wstring &wstr);
wstring EncodeWStringFromStringOfBytes(string &bytestr);
vector<string> GetParams(string &str, char splitChar);

// Macro to define property with custom getter and setter.
#define Property(GET, SET) _declspec(property(get = GET, put = SET))
#define ReadonlyProperty(GET) _declspec(property(get = GET))

namespace DB
{
	struct SUPPLY
	{
		uint ammoPerUnit;
		uint batsPerUnit;
		uint botsPerUnit;
		uint cloakBatsPerUnit;
		uint hullPerUnit;
	};

	struct CARGO_ITEM
	{
		uint archID;
		uint count;

		CARGO_ITEM(uint ArchID, uint Count)
		{
			archID = ArchID;
			count = Count;
		}
	};

	struct SHIP_LOCATION
	{
		uint baseID;
		uint systemID;
		Vector pos;
		Vector rot;
	};

	struct MODULE_ARCH
	{
		int maxCargoCapacity; // Above how many cargo capacity will docking ship be rejected.
		uint dockingTime; // Never make it lower than 1 or bugs occur.
		uint basicResupplyTime; // -1 to disable resupplying for the module.
		uint minCrewLimit; // 0 to allow resupplying without crew.
		uint dockDisatnce; // How far you need to be to dock.
		uint undockDistance; // How high will ship appear above carrier at undock.
	};

	struct ID_TRAITS
	{
		float crewLimitMultiplier; // Set to 0 to not require crew for resupplying for specific ID.
		vector<uint> suitableCrewIDs; // You can make crew relevant only for the ID.
		map<uint, SUPPLY> supplyItems; // You can force supply items to work only for that ID.
		string proxyBaseSuffix;	// Yes, you can provide different proxy bases for different IDs.
	};

	struct MODULE_CACHE
	{
		uint archID;
		wstring occupiedBy;

		MODULE_CACHE(uint ArchID, const wstring& OccupiedBy)
		{
			archID = ArchID;
			occupiedBy = OccupiedBy;
		}

		MODULE_CACHE(uint ArchID)
		{
			archID = ArchID;
		}
	};

	struct CLIENT_INFO
	{
		ID_TRAITS dockingTraits;
		vector<MODULE_CACHE> Modules;
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// The WatcherDB
	// Class that provides performant way to count installed docking modules on every ship at every moment of time.
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class WatcherDB
	{
	public:
		CLIENT_INFO Cache[MAX_CLIENT_ID + 1];
		map<uint, MODULE_ARCH> moduleArchInfo;
		map<uint, ID_TRAITS> IDTraits;

		void ReleaseModule(uint carrierClientID, wstring &dockedCharname);
		void OccupyModule(uint carrierClientID, uint moduleArch, wstring &dockingCharname);
	};

	// Instance of WatcherDB.
	extern WatcherDB Watcher;


	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Docking Modules Database
	// Provides convenient way to access values in files or in HookExt plugin as regular C++ variables.
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class OnlineData
	{
	private:
		uint iClientID;

	public:
		void DockedWith_Set(wstring &setVal) { HookExt::IniSetS(iClientID, "DockedWith", DecodeWStringToStringOfBytes(setVal)); }
		wstring DockedWith_Get() { return EncodeWStringFromStringOfBytes(HookExt::IniGetS(iClientID, "DockedWith")); }
		Property(DockedWith_Get, DockedWith_Set) wstring DockedWith;

		void DockedToModule_Set(uint setVal) { HookExt::IniSetI(iClientID, "DockedToModule", setVal); }
		uint DockedToModule_Get() { return HookExt::IniGetI(iClientID, "DockedToModule"); }
		Property(DockedToModule_Get, DockedToModule_Set) uint DockedToModule;

		void saveLastBaseID_Set(uint setVal) { HookExt::IniSetI(iClientID, "saveLastBaseID", setVal); }
		uint saveLastBaseID_Get() { return HookExt::IniGetI(iClientID, "saveLastBaseID"); }
		Property(saveLastBaseID_Get, saveLastBaseID_Set) uint saveLastBaseID;

		void LastPOBID_Set(uint setVal) { HookExt::IniSetI(iClientID, "base.last_player_base", setVal); }
		uint LastPOBID_Get() { return HookExt::IniGetI(iClientID, "base.last_player_base"); }
		Property(LastPOBID_Get, LastPOBID_Set) uint LastPOBID;

		void POBID_Set(uint setVal) { HookExt::IniSetI(iClientID, "base.player_base", setVal); }
		uint POBID_Get() { return HookExt::IniGetI(iClientID, "base.player_base"); }
		Property(POBID_Get, POBID_Set) uint POBID;

		void saveLastPOBID_Set(uint setVal) { HookExt::IniSetI(iClientID, "saveLastPOBID", setVal); }
		uint saveLastPOBID_Get() { return HookExt::IniGetI(iClientID, "saveLastPOBID"); }
		Property(saveLastPOBID_Get, saveLastPOBID_Set) uint saveLastPOBID;

		bool HasDockingModules_Get() { return !Watcher.Cache[iClientID].Modules.empty(); }
		ReadonlyProperty(HasDockingModules_Get) bool HasDockingModules;

		void DockedChars_Add(MODULE_CACHE &info);
		void DockedChars_Erase(MODULE_CACHE &module);
		void DockedChars_Remove(wstring &charname);
		void DockedChars_Clear();
		bool DockedChars_Empty();
		vector<MODULE_CACHE> DockedChars_Get();

		OnlineData(uint clientID)
		{
			iClientID = clientID;
		}
	};


	class CargoDB
	{
	private:
		vector<CARGO_ITEM> cargo;

	public:
		const vector<CARGO_ITEM>& Get() { return cargo; }
		void Clear() { cargo.clear(); }

		CargoDB() { }
		CargoDB(vector<CARGO_ITEM> &Cargo)
		{
			cargo = Cargo;
		}
	};

	class OfflineData
	{
	public:
		string path;
		wstring Charname;

		SHIP_LOCATION Location;
		vector<MODULE_CACHE> DockedChars;
		wstring DockedWith;
		uint DockedToModule;
		bool IsDocked;
		uint saveLastBaseID;
		uint saveLastPOBID;
		uint POBID;
		uint LastPOBID;

		CargoDB Cargo;

		OfflineData(wstring &charname);
		void Save();
	};


	class ClientsDB
	{
	public:
		OnlineData operator [] (uint iClientID)
		{
			return OnlineData(iClientID);
		};

		const OfflineData operator [] (wstring &charname)
		{
			return OfflineData(charname);
		};
	};

	// Instance of ClientsDB.
	extern ClientsDB Clients;
}
