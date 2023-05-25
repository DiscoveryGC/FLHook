#ifndef __MAIN_H__
#define __MAIN_H__ 1

#include <windows.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <math.h>
#include <list>
#include <set>
#include <map>
#include <algorithm>
#include <unordered_map>
#include <FLHook.h>
#include <plugin.h>
#include <PluginUtilities.h>

using namespace std;

static uint STORAGE_MODULE_CAPACITY = 40000;
void LogCheater(uint client, const wstring &reason);
uint GetAffliationFromClient(uint client);

struct RECIPE
{
	RECIPE() : produced_item(0), cooking_rate(0) {}
	uint nickname;
	uint produced_item;
	wstring infotext;
	uint cooking_rate;
	map<uint, uint> consumed_items;
	uint reqlevel;
};

struct BASE_VULNERABILITY_WINDOW {
	uint start;
	uint end;
};

struct WEAR_N_TEAR_MODIFIER{
	float fromHP;
	float toHP;
	float modifier;
};

struct ARCHTYPE_STRUCT
{
	int logic;
	int invulnerable;
	float radius;
	list<string> modules;
	int idrestriction;
	list<uint> allowedids;
	int shipclassrestriction;
	list<uint> allowedshipclasses;
	int isjump;
	bool display;
	bool mining;
	string miningevent;
};

struct MARKET_ITEM
{
	MARKET_ITEM() : quantity(0), price(1.0f), min_stock(100000), max_stock(100000) {}

	// Number of units of commodity stored in this base
	uint quantity;

	// Buy/Sell price for commodity.
	float price;

	// Stop selling if the base holds less than this number of items
	uint min_stock;

	// Stop buying if the base holds more than this number of items
	uint max_stock;
};

struct NEWS_ITEM
{
	wstring headline;
	wstring text;
};

class PlayerBase;

class Module
{
public:
	int type;
	int mining;
	static const int TYPE_BUILD = 0;
	static const int TYPE_CORE = 1;
	static const int TYPE_SHIELDGEN = 2;
	static const int TYPE_STORAGE = 3;
	static const int TYPE_DEFENSE_1 = 4;
	static const int TYPE_M_DOCKING = 5;
	static const int TYPE_M_JUMPDRIVES = 6;
	static const int TYPE_M_HYPERSPACE_SCANNER = 7;
	static const int TYPE_M_CLOAK = 8;
	static const int TYPE_DEFENSE_2 = 9;
	static const int TYPE_DEFENSE_3 = 10;
	static const int TYPE_M_CLOAKDISRUPTOR = 11;
	static const int TYPE_LAST = TYPE_M_CLOAKDISRUPTOR;

	Module(uint the_type) : type(the_type) {}
	virtual ~Module() {}
	virtual void Spawn() {}
	virtual wstring GetInfo(bool xml) = 0;
	virtual void LoadState(INI_Reader &ini) = 0;
	virtual void SaveState(FILE *file) = 0;

	virtual bool Timer(uint time) { return false; }

	virtual float SpaceObjDamaged(uint space_obj, uint attacking_space_obj, float curr_hitpoints, float new_hitpoints) { return 0.0f; }
	virtual bool SpaceObjDestroyed(uint space_obj) { return false; }
	virtual void SetReputation(int player_rep, float attitude) {}

};

class CoreModule : public Module
{
public:
	PlayerBase *base;

	// The space ID of this base
	uint space_obj;

	// If true, do not use food and commodities
	bool dont_eat;

	// If true, do not take damage
	bool dont_rust;

	// The list of goods and usage of goods per minute for the autosys effect
	map<uint, uint> mapAutosysGood;

	// The list of goods and usage of goods per minute for the autosys effect
	map<uint, uint> mapHumansysGood;

	CoreModule(PlayerBase *the_base);
	~CoreModule();
	void Spawn();
	wstring GetInfo(bool xml);

	void LoadState(INI_Reader &ini);
	void SaveState(FILE *file);

	bool Timer(uint time);
	float SpaceObjDamaged(uint space_obj, uint attacking_space_obj, float curr_hitpoints, float new_hitpoints);
	bool SpaceObjDestroyed(uint space_obj);
	void SetReputation(int player_rep, float attitude);
	float FindWearNTearModifier(float currHpPercentage);

	void RepairDamage(float max_base_health);
};

class ShieldModule : public Module
{
public:
	PlayerBase *base;

	// If true then a player has entered the system and so we reset the fuse
	// so that they see the shield
	bool reset_needed;

	ShieldModule(PlayerBase *the_base);
	~ShieldModule();
	wstring GetInfo(bool xml);

	void LoadState(INI_Reader &ini);
	void SaveState(FILE *file);

	bool Timer(uint time);
	void SetReputation(int player_rep, float attitude);

	bool HasShieldPower();
};

class StorageModule : public Module
{
public:
	PlayerBase *base;

	StorageModule(PlayerBase *the_base);
	~StorageModule();
	wstring GetInfo(bool xml);

	void LoadState(INI_Reader &ini);
	void SaveState(FILE *file);
};

class DefenseModule : public Module
{
public:
	PlayerBase *base;

	// The space object of the platform
	uint space_obj;

	// The position of the platform
	Vector pos;

	// The orientation of the platform
	Vector rot;

	DefenseModule(PlayerBase *the_base);
	DefenseModule(PlayerBase *the_base, uint the_type);
	~DefenseModule();
	wstring GetInfo(bool xml);

	void LoadState(INI_Reader &ini);
	void SaveState(FILE *file);

	bool Timer(uint time);
	float SpaceObjDamaged(uint space_obj, uint attacking_space_obj, float curr_hitpoints, float new_hitpoints);
	bool SpaceObjDestroyed(uint space_obj);
	void SetReputation(int player_rep, float attitude);
	void Reset();
};

class BuildModule : public Module
{
public:
	PlayerBase *base;

	int build_type;

	RECIPE active_recipe;

	BuildModule(PlayerBase *the_base);
	BuildModule(PlayerBase *the_base, uint the_building_type);

	wstring GetInfo(bool xml);

	bool Paused = false;
	void LoadState(INI_Reader &ini);
	void SaveState(FILE *file);

	bool Timer(uint time);
};

class FactoryModule : public Module
{
public:
	PlayerBase *base;

	// The currently active recipe
	RECIPE active_recipe;

	// List of queued recipes;
	list<uint> build_queue;

	FactoryModule(PlayerBase *the_base);
	FactoryModule(PlayerBase *the_base, uint type);
	wstring GetInfo(bool xml);
	void LoadState(INI_Reader &ini);
	void SaveState(FILE *file);
	bool Timer(uint time);

	bool Paused = false;
	bool ToggleQueuePaused(bool NewState);
	bool AddToQueue(uint the_equipment_type);
	bool ClearQueue();
	void ClearRecipe();
};

class BasePassword
{
public:
	wstring pass;
	bool admin = false;
	bool viewshop = false;

	friend bool operator ==(const BasePassword& lhs, const BasePassword& rhs);
};
inline bool operator ==(const BasePassword& lhs, const BasePassword& rhs)
{
	return lhs.pass == rhs.pass;
};

class PlayerBase
{
public:
	PlayerBase(uint client, const wstring &password, const wstring &basename);
	PlayerBase(const string &path);
	~PlayerBase();

	void Spawn();

	bool Timer(uint curr_time);

	void SetupDefaults();
	void Load();
	void Save();

	bool AddMarketGood(uint good, uint quantity);
	void RemoveMarketGood(uint good, uint quantity);
	void ChangeMoney(INT64 quantity);
	uint GetRemainingCargoSpace();
	uint GetMaxCargoSpace();
	uint HasMarketItem(uint good);

	static string CreateBaseNickname(const string &basename);

	float GetAttitudeTowardsClient(uint client, bool emulated_siege_mode = false);
	void SyncReputationForBase();
	void SiegeModChainReaction(uint client);
	void SyncReputationForBaseObject(uint space_obj);

	float SpaceObjDamaged(uint space_obj, uint attacking_space_obj, float curr_hitpoints, float new_hitpoints);

	// The base nickname
	string nickname;

	// The base affiliation
	uint affiliation;

	//base type
	string basetype;

	//base appearance
	string basesolar;

	//base appearance
	string baseloadout;
	// The name of the base shown to other players
	wstring basename;

	// The infocard for the base
	wstring infocard;

	// The infocard paragraphs for the base
#define MAX_CHARACTERS 500
#define MAX_PARAGRAPHS 5
	wstring infocard_para[MAX_PARAGRAPHS + 1];

	// The system the base is in
	uint system;

	// The position of the base
	Vector position;

	// The rotation of the base
	Matrix rotation;

	// The basic armour and commodity storage available on this base->
	uint base_level;

	// The commodities carried by this base->
	map<uint, MARKET_ITEM> market_items;

	// The money this base has
	INT64 money;

	// The current hit points of the base
	float base_health;

	// The maximum hit points of the base
	float max_base_health;

	// When the base is spawned, this is the IDS of the base name
	uint solar_ids;

	// The ingame hash of the nickname
	uint base;

	map<wstring, uint> last_login_attempt_time;
	map<wstring, int> unsuccessful_logins_in_a_row;

	// The list of administration passwords
	list<BasePassword> passwords;

	// If 0 then base is neutral to all ships. Only ships on the ally tag list may dock.
	// If 1 then base is hostile to all ships unless they are on the ally tag list.
	// If 2 then base is neutral to all ships and any ship may dock.
	int defense_mode;

	//changes how defense mod act depending on the amount of damage made to base in the last hours
	bool siege_mode;

	//shield strength parameters
	float shield_strength_multiplier;
	float base_shield_reinforcement_threshold;
	float damage_taken_since_last_threshold;

	// List of allied ship tags.
	list<wstring> ally_tags;

	//List of allied factions
	set<uint> ally_factions;

	//List of hostile factions
	set<uint> hostile_factions;

	// List of ships that are hostile to this base
	map<wstring, wstring> hostile_tags;
	map<wstring, float> hostile_tags_damage;

	// List of ships that are permanently hostile to this base
	list<wstring> perma_hostile_tags;

	// Modules for base
	vector<Module*> modules;

	// Path to base ini file.
	string path;

	// The proxy base associated with the system this base is in.
	uint proxy_base;

	// if true, the base was repaired or is able to be repaired
	bool repairing;

	// The state of the shield
	static const int SHIELD_STATE_OFFLINE = 0;
	static const int SHIELD_STATE_ONLINE = 1;
	static const int SHIELD_STATE_ACTIVE = 2;
	int shield_state;

	// The number of seconds that shield will be active
	int shield_active_time;

	// When this timer drops to less than 0 the base is saved	 
	int save_timer;

	int logic;
	int invulnerable;

	//last player attacker
	wstring last_attacker;

	////////////Unique to Solars/////////////
	//the radius (for jumps)
	float radius;

	//the destination system, once again for jumps
	uint destsystem;

	//the destination vector
	Vector destposition;

	/////////////////////////////////////////
};

PlayerBase *GetPlayerBase(uint base);
PlayerBase *GetPlayerBaseForClient(uint client);

void BaseLogging(const char *szString, ...);

void SaveBases();
void DeleteBase(PlayerBase *base);
void LoadDockState(uint client);
void SaveDockState(uint client);
void DeleteDockState(uint client);

/// Send a command to the client at destination ID 0x9999
void SendCommand(uint client, const wstring &message);
void SendSetBaseInfoText(uint client, const wstring &message);
void SendSetBaseInfoText2(uint client, const wstring &message);
void SendResetMarketOverride(uint client);
void SendMarketGoodUpdated(PlayerBase *base, uint good, MARKET_ITEM &item);
void SendMarketGoodSync(PlayerBase *base, uint client);
void SendBaseStatus(uint client, PlayerBase *base);
void SendBaseStatus(PlayerBase *base);
void ForceLaunch(uint client);

struct CLIENT_DATA
{
	CLIENT_DATA() : reverse_sell(false), stop_buy(false), admin(false),
		player_base(0), last_player_base(0) {}

	// If true reverse the last sell by readding the item.
	bool reverse_sell;

	// The cargo list used by the reverse sell.
	list<CARGO_INFO> cargo;

	// If true block the current buy and associated reqitemadd function.
	bool stop_buy;

	// True if this player is the base administrator.
	bool admin;

	// True if this player is able to view the shop.
	bool viewshop;

	// Set to player base hash if ship is in base-> 0 if not.
	uint player_base;

	// Set to player base hash if ship is in base or was last in a player base-> 0 after 
	// docking at any non player base->
	uint last_player_base;
};

namespace ExportData
{
	void ToHTML();
	void ToJSON();
}

namespace Siege
{
	void SiegeGunDeploy(uint client, const wstring &args);
	int CalculateHealthPercentage(uint basehash, int health, int maxhealth);
	void SiegeAudioNotification(uint iClientID, int level);
	void SiegeAudioCalc(uint basehash, uint iSystemID, Vector pos, int level);
	int GetRandomSound(int min, int max);
}

namespace AP
{
	void SwitchSystem(uint iClientID, uint system, Vector pos, Matrix ornt);
	bool SystemSwitchOutComplete(unsigned int iShip, unsigned int iClientID);
	void ClearClientInfo(unsigned int iClientID);
}

namespace PlayerCommands
{
	void BaseHelp(uint client, const wstring &args);

	void BaseLogin(uint client, const wstring &args);
	void BaseAddPwd(uint client, const wstring &args);
	void BaseRmPwd(uint client, const wstring &args);
	void BaseLstPwd(uint client, const wstring &args);
	void BaseSetMasterPwd(uint client, const wstring &args);

	void BaseAddAllyTag(uint client, const wstring &args);
	void BaseRmAllyTag(uint client, const wstring &args);
	void BaseLstAllyTag(uint client, const wstring &args);
	void BaseAddAllyFac(uint client, const wstring &args, bool HostileFactionMod = false);
	void BaseRmAllyFac(uint client, const wstring &args, bool HostileFactionMod = false);
	void BaseClearAllyFac(uint client, const wstring &args, bool HostileFactionMod = false);
	void BaseLstAllyFac(uint client, const wstring &args, bool HostileFactionMod = false);
	void BaseViewMyFac(uint client, const wstring &args);
	void BaseAddHostileTag(uint client, const wstring &args);
	void BaseRmHostileTag(uint client, const wstring &args);
	void BaseLstHostileTag(uint client, const wstring &args);
	void BaseRep(uint client, const wstring &args);

	void BaseInfo(uint client, const wstring &args);
	void BaseDefenseMode(uint client, const wstring &args);
	void BaseDefMod(uint client, const wstring &args);
	void BaseBuildMod(uint client, const wstring &args);
	void BaseFacMod(uint client, const wstring &args);
	void BaseShieldMod(uint client, const wstring &args);
	void Bank(uint client, const wstring &args);
	void Shop(uint client, const wstring &args);
	void GetNecessitiesStatus(uint client, const wstring &args);

	void BaseDeploy(uint client, const wstring &args);

	void Aff_initer();
}

namespace Log {
	void LogBaseAction(string basename, const char *message);
	void LogGenericAction(string message);
}

extern map<uint, CLIENT_DATA> clients;

extern unordered_map<uint, Module*> spaceobj_modules;

// Map of ingame hash to info
extern map<uint, class PlayerBase*> player_bases;
extern map<uint, PlayerBase*>::iterator baseSaveIterator;

struct POBSOUNDS
{
	uint destruction1;
	uint destruction2;
	uint heavydamage1;
	uint heavydamage2;
	uint heavydamage3;
	uint mediumdamage1;
	uint mediumdamage2;
	uint lowdamage1;
	uint lowdamage2;
};

extern POBSOUNDS pbsounds;

extern int set_plugin_debug;

extern map<uint, RECIPE> recipes;

struct REPAIR_ITEM
{
	uint good;
	uint quantity;
};
extern list<REPAIR_ITEM> set_base_repair_items;

extern uint set_base_crew_type;

extern map<uint, uint> set_base_crew_consumption_items;

extern map<uint, uint> set_base_crew_food_items;

extern map<string, ARCHTYPE_STRUCT> mapArchs;

/// The ship used to construct and upgrade bases
extern uint set_construction_shiparch;

/// Map of good to quantity for items required by construction ship
extern map<uint, uint> construction_items;

/// Map of item nickname hash to recipes to operate shield.
extern map<uint, uint> shield_power_items;

/// Damage to the base every 10 seconds
extern uint set_damage_per_10sec;

/// Damage to the base every tick
extern uint set_damage_per_tick;

/// Damage multiplier for damaged/abandoned stations
/// In case of overlapping modifiers, only the first one specified in .cfg file will apply
extern list<WEAR_N_TEAR_MODIFIER> wear_n_tear_mod_list;

/// Additional damage penalty for stations without proper crew
extern float no_crew_damage_multiplier;

/// How much damage to repair per cycle
extern uint repair_per_repair_cycle;

/// The seconds per damage tick
extern uint set_damage_tick_time;

/// The seconds per tick
extern uint set_tick_time;

// set of configurable variables defining the diminishing returns on damage during POB siege
// POB starts at base_shield_strength, then every 'threshold' of damage taken, 
// shield goes up in absorption by the 'increment'
// threshold size is to be configured per core level.
extern map<int, float> shield_reinforcement_threshold_map;
extern float shield_reinforcement_increment;
extern float base_shield_strength;

extern bool isGlobalBaseInvulnerabilityActive;

bool checkBaseVulnerabilityStatus();

/// Holiday mode
extern bool set_holiday_mode;

wstring HtmlEncode(wstring text);

extern string set_status_path_html;
extern string set_status_path_json;

extern const char* MODULE_TYPE_NICKNAMES[13];

extern float damage_threshold;

extern float siege_mode_damage_trigger_level;

extern float siege_mode_chain_reaction_trigger_distance;
#endif
