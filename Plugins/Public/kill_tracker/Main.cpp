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


PLUGIN_RETURNCODE returncode;

static std::array<std::array<float, MAX_CLIENT_ID + 1>, MAX_CLIENT_ID + 1> damageArray;

IMPORT uint iDmgTo;
IMPORT float g_LastHitPts;

bool showPercentage = true;

//! Message broadcasted systemwide upon ship's death
//! {0} is replaced with victim's name, {1} with player who dealt the most damage to them,
//! {2} with percentage of hull damage taken byt that player.
std::wstring deathDamageTemplate = L"{0} took most hull damage from {1}";
std::wstring deathPercentageDone = L", {2}%";

// Load Settings
void __stdcall LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;
	for (auto& subArray : damageArray)
		subArray.fill(0.0f);

	// The path to the configuration file.
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\kill_tracker.cfg";

	INI_Reader ini;
	if (ini.open(scPluginCfgFile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("General"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("ShowPercentage"))
					{
						showPercentage = ini.get_value_bool(0);
					}
				}
			}
		}
	}
}

/** @ingroup KillTracker
 * @brief Called when a player types "/kills".
 */
void UserCmd_Kills(uint client, const std::wstring& wscParam)
{
	std::wstring targetCharName = GetParam(wscParam, ' ', 1);
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
	int rank;
	pub::Player::GetNumKills(clientId, kills);
	pub::Player::GetRank(clientId, rank);
	PrintUserCmdText(client, L"PvP kills: %u", kills);
	PrintUserCmdText(client, L"Level: %u", rank);
}

uint GetClientIdByShip(uint shipId)
{
	PlayerData* pd = nullptr;
	while (pd = Players.traverse_active(pd))
	{
		if (pd->iShipID == shipId)
			return pd->iOnlineID;
	}
	return 0;
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
			uint killerId = GetClientIdByShip(lastInflictorId);

			if (killerId && killerId != client)
			{
				int kills;
				pub::Player::GetNumKills(killerId, kills);
				kills++;
				pub::Player::SetNumKills(killerId, kills);
			}
		}
	}
}

void __stdcall AddDamageEntry(DamageList* damageList, ushort subObjId, float& newHitPoints, enum DamageEntry::SubObjFate fate)
{
	returncode = DEFAULT_RETURNCODE;
	if (iDmgTo && subObjId == 1)
	{
		const auto& inflictor = damageList->iInflictorPlayerID;
		if (inflictor && iDmgTo && inflictor != iDmgTo)
		{
			damageArray[inflictor][iDmgTo] += g_LastHitPts;
		}
	}
}

void clearDamageTaken(uint victim)
{
	for (auto& damageEntry : damageArray[victim])
		damageEntry = 0.0f;
}

void clearDamageDone(uint inflictor)
{
	for (int i = 1; i < MAX_CLIENT_ID + 1; i++)
		damageArray[i][inflictor] = 0.0f;
}

void __stdcall SendDeathMessage(const std::wstring& message, uint system, uint clientVictim, uint clientKiller)
{
	returncode = DEFAULT_RETURNCODE;

	if (clientVictim && clientKiller)
	{
		uint greatestInflictorId = 0;
		float greatestDamageDealt = 0.0f;
		float totalDamageTaken = 0.0f;
		for (uint inflictorIndex = 1; inflictorIndex < damageArray[0].size(); inflictorIndex++)
		{
			float damageDealt = damageArray[inflictorIndex][clientVictim];
			totalDamageTaken += damageDealt;
			if (damageDealt > greatestDamageDealt)
			{
				greatestDamageDealt = damageDealt;
				greatestInflictorId = inflictorIndex;
			}
		}
		clearDamageTaken(clientVictim);
		if (totalDamageTaken == 0.0f || greatestInflictorId == 0)
			return;

		std::wstring victimName = (const wchar_t*)Players.GetActiveCharacterName(clientVictim);
		std::wstring greatestInflictorName = (const wchar_t*)Players.GetActiveCharacterName(greatestInflictorId);

		std::wstring greatestDamageMessage = deathDamageTemplate;
		greatestDamageMessage = ReplaceStr(greatestDamageMessage, L"{0}", victimName);
		greatestDamageMessage = ReplaceStr(greatestDamageMessage, L"{1}", greatestInflictorName);
		if(showPercentage)
			greatestDamageMessage += ReplaceStr(deathPercentageDone, L"{2}", stows(itos(static_cast<uint>(ceil((greatestDamageDealt / totalDamageTaken) * 100)))));

		ushort cFormat = 0;
		wchar_t wszFormatBuf[8];
		swprintf(wszFormatBuf, _countof(wszFormatBuf), L"%02X", (long)cFormat);
		wstring wscTRADataFormat = wszFormatBuf;

		greatestDamageMessage = L"<TRA data=\"0xFF8C00" + wscTRADataFormat +
			L"\" mask=\"-1\"/><TEXT>" + XMLText(greatestDamageMessage) + L"</TEXT>";
		
		HkFMsgS(stows(itos(system)), greatestDamageMessage);
	}
}

void __stdcall Disconnect(uint client, enum EFLConnection conn)
{
	returncode = DEFAULT_RETURNCODE;
	clearDamageTaken(client);
	clearDamageDone(client);
}

void __stdcall PlayerLaunch(uint shipId, uint client)
{
	returncode = DEFAULT_RETURNCODE;
	clearDamageTaken(client);
	clearDamageDone(client);
	float maxHp = Archetype::GetShip(Players[client].iShipArchetype)->fHitPoints;
}

void __stdcall CharacterSelect(CHARACTER_ID const& cid, uint client)
{
	returncode = DEFAULT_RETURNCODE;
	clearDamageTaken(client);
	clearDamageDone(client);
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
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&Disconnect, PLUGIN_HkIServerImpl_DisConnect, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&PlayerLaunch, PLUGIN_HkIServerImpl_PlayerLaunch, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&CharacterSelect, PLUGIN_HkIServerImpl_CharacterSelect, 0));

	return p_PI;
}