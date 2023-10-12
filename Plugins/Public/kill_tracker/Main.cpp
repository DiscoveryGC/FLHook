/**
 * @date Unknown
 * @author ||KOS||Acid (Ported by Raikkonen)
 * @defgroup KillTracker Kill Tracker
 * @brief
 * This plugin is used to count pvp kills and save them in the player file. Vanilla doesn't do this by default.
 * Also keeps track of damage taken between players, prints greatest damage contributor.
 *
 * @paragraph cmds Player Commands
 * All commands are prefixed with '/' unless explicitly specified.
 * - kills {client} - Shows the pvp kills for a player if a client id is specified, or if not, the player who typed it.
 *
 * @paragraph adminCmds Admin Commands
 * There are no admin commands in this plugin.
 *
 * @paragraph configuration Configuration
 * No configuration file is needed.
 *
 * @paragraph ipc IPC Interfaces Exposed
 * This plugin does not expose any functionality.
 *
 * @paragraph optional Optional Plugin Dependencies
 * This plugin has no dependencies.
 */

#include <FLHook.h>
#include <plugin.h>
#include <PluginUtilities.h>
#include <array>
#include <unordered_map>
#include <set>


PLUGIN_RETURNCODE returncode;

struct DamageDoneStruct
{
	float currDamage = 0.0f;
	float lastUndockDamage = 0.0f;
};

static array<array<DamageDoneStruct, MAX_CLIENT_ID + 1>, MAX_CLIENT_ID + 1> damageArray;

IMPORT uint iDmgTo;
IMPORT float g_LastHitPts;

//! Message broadcasted systemwide upon ship's death
//! {0} is replaced with victim's name, {1} with player who dealt the most damage to them,
//! {2} with percentage of hull damage taken byt that player.
wstring defaultDeathDamageTemplate = L"%victim died to %killer";
wstring killMsgStyle = L"0xCC303001"; // default: blue, bold
unordered_map<uint, vector<wstring>> shipClassToDeathMsgListMap;

uint numberOfKillers = 3;
float deathBroadcastRange = 15000;
uint minimumAssistPercentage = 5;

// Load Settings
void __stdcall LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	srand(time(nullptr));

	for (auto& subArray : damageArray)
	{
		subArray.fill({ 0.0f, 0.0f });
	}
	// The path to the configuration file.
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + R"(\flhook_plugins\kill_tracker.cfg)";

	INI_Reader ini;
	if (ini.open(scPluginCfgFile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("General"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("DeathBroadcastRange"))
					{
						deathBroadcastRange = ini.get_value_float(0);
					}
					else if (ini.is_value("NumberOfKillers"))
					{
						numberOfKillers = ini.get_value_int(0);
					}
					else if (ini.is_value("KillMsgStyle"))
					{
						killMsgStyle = stows(ini.get_value_string());
					}
				}
			}
			else if (ini.is_header("KillMessage"))
			{
				vector<wstring> deathMsgList;
				uint shipClass;
				while (ini.read_value())
				{
					if (ini.is_value("ShipType"))
					{
						string typeStr = ToLower(ini.get_value_string(0));
						if (typeStr == "fighter")
							shipClass = OBJ_FIGHTER;
						else if (typeStr == "freighter")
							shipClass = OBJ_FREIGHTER;
						else if (typeStr == "transport")
							shipClass = OBJ_TRANSPORT;
						else if (typeStr == "gunboat")
							shipClass = OBJ_GUNBOAT;
						else if (typeStr == "cruiser")
							shipClass = OBJ_CRUISER;
						else if (typeStr == "capital")
							shipClass = OBJ_CAPITAL;
						else
							ConPrint(L"KillTracker: Error reading config for Death Messages, value %ls not recognized\n", stows(typeStr).c_str());
					}
					else if (ini.is_value("DeathMsg"))
					{
						deathMsgList.push_back(stows(ini.get_value_string()));
					}
				}

				shipClassToDeathMsgListMap[shipClass] = deathMsgList;
			}
		}
	}
}

/** @ingroup KillTracker
 * @brief Called when a player types "/kills".
 */
void UserCmd_Kills(const uint client, const wstring& wscParam)
{
	wstring targetCharName = GetParam(wscParam, ' ', 1);
	uint clientId;

	if (!targetCharName.empty())
	{
		clientId = HkGetClientIdFromCharname(targetCharName);
		if (!clientId)
		{
			PrintUserCmdText(client, L"Player not found");
			return;
		}
	}
	else
	{
		clientId = client;
	}
	int kills;
	pub::Player::GetNumKills(clientId, kills);
	PrintUserCmdText(client, L"PvP kills: %u", kills);
}

/** @ingroup KillTracker
 * @brief Hook on ShipDestroyed. Increments the number of kills of a player if there is one.
 */
void __stdcall ShipDestroyed(DamageList* dmg, DWORD* ecx, uint kill)
{
	returncode = DEFAULT_RETURNCODE;
	if (kill == 1)
	{
		const CShip* cShip = reinterpret_cast<CShip*>(ecx[4]);

		if (uint client = cShip->GetOwnerPlayer())
		{
			uint lastInflictorId = dmg->get_cause() == 0 ? ClientInfo[client].dmgLast.get_inflictor_id() : dmg->get_inflictor_id();
			uint killerId = HkGetClientIDByShip(lastInflictorId);

			if (killerId && killerId != client)
			{
				int kills;
				pub::Player::GetNumKills(killerId, kills);
				pub::Player::SetNumKills(killerId, ++kills);
			}
		}
	}
}

void __stdcall AddDamageEntry(DamageList* damageList, ushort subObjId, float& newHitPoints, enum DamageEntry::SubObjFate fate)
{
	returncode = DEFAULT_RETURNCODE;
	if (iDmgTo && subObjId == 1 && g_LastHitPts > newHitPoints) //ignore negative hp events such as repair ship
	{
		const auto& inflictor = damageList->iInflictorPlayerID;
		if (inflictor && inflictor != iDmgTo)
		{
			damageArray[iDmgTo][inflictor].currDamage += g_LastHitPts - newHitPoints;
		}
	}
}

void ClearDamageTaken(const uint victim)
{
	damageArray[victim].fill({ 0.0f, 0.0f });
}

void ClearDamageDone(const uint inflictor, const bool isFullReset)
{
	for (int i = 1; i <= MAX_CLIENT_ID; i++)
	{
		auto& damageData = damageArray[i][inflictor];
		if (isFullReset)
		{
			damageData.lastUndockDamage = 0.0f;
		}
		else
		{
			damageData.lastUndockDamage = damageData.currDamage;
		}
		damageData.currDamage = 0.0f;
	}
}

void ProcessNonPvPDeath(const wstring& message, const uint system)
{
	wstring deathMessage = L"<TRA data=\"0x0000CC01" // Red, Bold
		L"\" mask=\"-1\"/><TEXT>" + XMLText(message) + L"</TEXT>";

	PlayerData* pd = nullptr;
	while (pd = Players.traverse_active(pd))
	{
		if (pd->iSystemID == system)
		{
			HkFMsg(pd->iOnlineID, deathMessage);
		}
	}
}

wstring SelectRandomDeathMessage(const uint iClientID)
{
	uint shipType = Archetype::GetShip(Players[iClientID].iShipArchetype)->iArchType;
	const auto& deathMsgList = shipClassToDeathMsgListMap[shipType];
	if (deathMsgList.empty())
	{
		return defaultDeathDamageTemplate;
	}
	else
	{
		return deathMsgList.at(rand() % deathMsgList.size());
	}
}

inline float GetDamageDone(const DamageDoneStruct& damageDone)
{
	if (damageDone.currDamage != 0.0f)
	{
		return damageDone.currDamage;
	}
	if(damageDone.lastUndockDamage != 0.0f)
	{
		return damageDone.lastUndockDamage;
	}
	return 0.0f;
}

void __stdcall SendDeathMessage(const wstring& message, uint system, uint clientVictim, uint clientKiller)
{
	returncode = DEFAULT_RETURNCODE;
	if (!clientVictim)
	{
		return;
	}

	returncode = NOFUNCTIONCALL;

	map<float, uint> damageToInflictorMap; // damage is the key instead of value because keys are sorted, used to render top contributors in order
	set<CPlayerGroup*> killerGroups;

	float totalDamageTaken = 0.0f;
	PlayerData* pd = nullptr;
	while (pd = Players.traverse_active(pd))
	{
		auto& damageData = damageArray[pd->iOnlineID][clientVictim];
		float damageToAdd = GetDamageDone(damageData);
		
		if (damageToAdd == 0.0f)
		{
			continue;
		}

		damageToInflictorMap[damageToAdd] = pd->iOnlineID;
		killerGroups.insert(pd->PlayerGroup);
		totalDamageTaken += damageToAdd;
	}
	if (totalDamageTaken == 0.0f)
	{
		ClearDamageTaken(clientVictim);
		ProcessNonPvPDeath(message, system);
		return;
	}

	wstring victimName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(clientVictim));

	wstring deathMessage = SelectRandomDeathMessage(clientVictim);
	deathMessage = ReplaceStr(deathMessage, L"%victim", victimName);
	wstring assistMessage = L"";

	uint killerCounter = 0;

	for (auto& i = damageToInflictorMap.rbegin(); i != damageToInflictorMap.rend(); i++) // top values at the end
	{
		if (i == damageToInflictorMap.rend() || killerCounter >= numberOfKillers)
		{
			break;
		}
		uint contributionPercentage = static_cast<uint>(round((i->first / totalDamageTaken) * 100));
		if (contributionPercentage < minimumAssistPercentage)
		{
			break;
		}

		wstring inflictorName = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(i->second));
		if (killerCounter == 0)
		{
			deathMessage = ReplaceStr(deathMessage, L"%killer", inflictorName);
			deathMessage += L" (" + stows(itos(contributionPercentage)) + L"%)";
		}
		else if (killerCounter == 1)
		{
			assistMessage = L"Assisted by: " + inflictorName + L" (" + stows(itos(contributionPercentage)) + L"%)";
		}
		else
		{
			assistMessage += L", " + inflictorName + L" (" + stows(itos(contributionPercentage)) + L"%)";
		}
		killerCounter++;
	}

	deathMessage = L"<TRA data=\"" + killMsgStyle +
		L"\" mask=\"-1\"/><TEXT>" + XMLText(deathMessage) + L"</TEXT>";
	if (!assistMessage.empty())
	{
		assistMessage = L"<TRA data=\"" + killMsgStyle +
			L"\" mask=\"-1\"/><TEXT>" + XMLText(assistMessage) + L"</TEXT>";
	}

	CPlayerGroup* victimGroup = Players[clientVictim].PlayerGroup;

	uint systemId;
	pub::Player::GetSystem(clientVictim, systemId);
	Vector victimPos;
	Matrix victimOri;
	pub::SpaceObj::GetLocation(Players[clientVictim].iShipID, victimPos, victimOri);

	pd = nullptr;
	while (pd = Players.traverse_active(pd))
	{
		uint playerId = pd->iOnlineID;
		if (GetDamageDone(damageArray[clientVictim][playerId]) != 0.0f)
		{
			HkFMsg(playerId, deathMessage);
			if (!assistMessage.empty())
			{
				HkFMsg(playerId, assistMessage);
			}
			continue;
		}
		if (
		(pd->PlayerGroup &&
		  ((victimGroup && victimGroup == pd->PlayerGroup)
		    || ( pd->PlayerGroup && killerGroups.count(pd->PlayerGroup))))
		|| playerId == clientVictim)
		{
			HkFMsg(playerId, deathMessage);
			if (!assistMessage.empty())
			{
				HkFMsg(playerId, assistMessage);
			}
			continue;
		}
		if (pd->iSystemID == systemId && Players[playerId].iShipID)
		{
			Vector pos;
			Matrix ori;
			pub::SpaceObj::GetLocation(Players[playerId].iShipID, pos, ori);
			if (HkDistance3D(victimPos, pos) <= deathBroadcastRange)
			{
				HkFMsg(playerId, deathMessage);
				if (!assistMessage.empty())
				{
					HkFMsg(playerId, assistMessage);
				}
			}
		}
	}

	ClearDamageTaken(clientVictim);
}

void __stdcall DelayedDisconnect(uint client)
{
	returncode = DEFAULT_RETURNCODE;
	ClearDamageTaken(client);
	ClearDamageDone(client, true);
}

void __stdcall Disconnect(uint client, enum EFLConnection conn)
{
	returncode = DEFAULT_RETURNCODE;
	ClearDamageTaken(client);
	ClearDamageDone(client, true);
}

void __stdcall PlayerLaunch(uint shipId, uint client)
{
	returncode = DEFAULT_RETURNCODE;
	ClearDamageTaken(client);
	ClearDamageDone(client, false);
}

void __stdcall CharacterSelect(CHARACTER_ID const& cid, uint client)
{
	returncode = DEFAULT_RETURNCODE;
	ClearDamageTaken(client);
	ClearDamageDone(client, true);
}


bool UserCmd_Process(uint client, const wstring &wscCmd)
{
	returncode = DEFAULT_RETURNCODE;

	if (wscCmd.find(L"/kills") == 0)
	{
		UserCmd_Kills(client, wscCmd);
		returncode = SKIPPLUGINS_NOFUNCTIONCALL;
	}
	return true;
}


EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "Kill Tracker";
	p_PI->sShortName = "killtracker";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ShipDestroyed, PLUGIN_ShipDestroyed, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&AddDamageEntry, PLUGIN_HkCb_AddDmgEntry_AFTER, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SendDeathMessage, PLUGIN_SendDeathMsg, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&DelayedDisconnect, PLUGIN_DelayedDisconnect, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Disconnect, PLUGIN_HkIServerImpl_DisConnect, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch, PLUGIN_HkIServerImpl_PlayerLaunch, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterSelect, PLUGIN_HkIServerImpl_CharacterSelect, 0));

	return p_PI;
}