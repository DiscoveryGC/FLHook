#include "main.h"

/// Send a command to the client at destination ID 0x9999
void SendCommand(uint client, const wstring& message)
{
	HkFMsg(client, L"<TEXT>" + XMLText(message) + L"</TEXT>");
}

void SendJumpObjOverride(uint client, uint jumpObjId, uint newTargetSystem)
{
	wchar_t buf[50];
	_snwprintf(buf, sizeof(buf), L" OverrideJumpObject %u %u", jumpObjId, newTargetSystem);
	SendCommand(client, buf);
}

void SendSetBaseInfoText(uint client, const wstring& message)
{
	SendCommand(client, wstring(L" SetBaseInfoText ") + message);
}

void SendSetBaseInfoText2(uint client, const wstring& message)
{
	SendCommand(client, wstring(L" SetBaseInfoText2 ") + message);
}

void SendResetMarketOverride(uint client)
{
	SendCommand(client, L" ResetMarketOverride");
}

// Send a price update to all clients in the player base for a single good
void SendMarketGoodUpdated(PlayerBase* base, uint good, MARKET_ITEM& item)
{
	struct PlayerData* pd = 0;
	while (pd = Players.traverse_active(pd))
	{
		uint client = pd->iOnlineID;
		if (!HkIsInCharSelectMenu(client))
		{
			if (clients[client].player_base == base->base)
			{
				// NB: If price is 0 it will not be shown at all.
				wchar_t buf[200];
				// If the base has none of the item then it is buy-only at the client.
				if (item.quantity == 0)
				{
					_snwprintf(buf, sizeof(buf), L" SetMarketOverride %u %u %f %u %u",
						base->proxy_base, good, item.price, 1, 0);
				}
				// If the item is buy only and this is not an admin then it is
				// buy only at the client
				else if (item.min_stock >= item.quantity && !clients[client].admin)
				{
					_snwprintf(buf, sizeof(buf), L" SetMarketOverride %u %u %f %u %u",
						base->proxy_base, good, item.price, 1, 0);
				}
				// Otherwise this item is for sale by the client.
				else
				{
					_snwprintf(buf, sizeof(buf), L" SetMarketOverride %u %u %f %u %u",
						base->proxy_base, good, item.price, 0, item.quantity);
				}
				SendCommand(client, buf);
			}
		}
	}
}

// Send a price update to a single client for all goods in the base
void SendMarketGoodSync(PlayerBase* base, uint client)
{
	// Reset the client's market
	SendResetMarketOverride(client);

	// Send a dummy entry if there are no goods at this base
	if (!base->market_items.size())
		SendCommand(client, L" SetMarketOverride 0 0 0 0");

	// Send the market
	for (auto i = base->market_items.begin();
		i != base->market_items.end(); i++)
	{
		uint good = i->first;
		MARKET_ITEM& item = i->second;
		wchar_t buf[200];
		// If the base has none of the item then it is buy-only at the client.
		if (item.quantity == 0)
		{
			_snwprintf(buf, sizeof(buf), L" SetMarketOverride %u %u %f %u %u",
				base->proxy_base, good, item.price, 1, 0);
		}
		// If the item is buy only and this is not an admin then it is
		// buy only at the client
		else if (item.min_stock >= item.quantity && !clients[client].admin)
		{
			_snwprintf(buf, sizeof(buf), L" SetMarketOverride %u %u %f %u %u",
				base->proxy_base, good, item.price, 1, 0);
		}
		// Otherwise this item is for sale by the client.
		else
		{
			_snwprintf(buf, sizeof(buf), L" SetMarketOverride %u %u %f %u %u",
				base->proxy_base, good, item.price, 0, item.quantity);
		}
		SendCommand(client, buf);
	}
}

wstring UIntToPrettyStr(uint value)
{
	wchar_t buf[1000];
	swprintf(buf, _countof(buf), L"%u", value);
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

static wstring Int64ToPrettyStr(INT64 iValue)
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

static wstring IntToStr(uint iValue)
{
	wchar_t buf[1000];
	swprintf(buf, _countof(buf), L"%u", iValue);
	return	buf;
}

void SendBaseStatus(uint client, PlayerBase* base)
{
	const Universe::ISystem* sys = Universe::get_system(base->system);

	wstring base_status = L"<RDL><PUSH/>";
	base_status += L"<TEXT>" + XMLText(base->basename) + L", " + HkGetWStringFromIDS(sys->strid_name) + L"</TEXT><PARA/>";
	
	wstring affiliation_string = L"";
	if (base->affiliation)
	{
		affiliation_string = HkGetWStringFromIDS(Reputation::get_name(base->affiliation));
	}
	else
	{
		affiliation_string = L"Unaffiliated";
	}

	base_status += L"<TEXT>Core " + IntToStr(base->base_level) + L" " + affiliation_string + L" Installation</TEXT><PARA/><PARA/>";

	base_status += base->infocard;

	base_status += L"<TEXT>Cargo Storage: " + Int64ToPrettyStr(base->GetRemainingCargoSpace()) + L" free of " + Int64ToPrettyStr(base->GetMaxCargoSpace()) + L"</TEXT><PARA/>";
	base_status += L"<TEXT>Money: " + Int64ToPrettyStr(base->money) + L"</TEXT><PARA/>";
	if (!base->invulnerable)
	{
		wstring max_hp_string;
		if ((INT64)base->max_base_health != INT64_MIN) // prevent bases with no defined maxHP from displaying "Max Hit Points: -9quintillion"
		{
			max_hp_string = Int64ToPrettyStr((INT64)base->max_base_health);
		}
		else
		{
			max_hp_string = L"Undefined";
		}
		base_status += L"<TEXT>Hit Points: " + Int64ToPrettyStr((INT64)base->base_health) + L" / " + max_hp_string + L"</TEXT><PARA/>";
	}
	else
	{
		base_status += L"<TEXT>Hit Points: Indestructible</TEXT><PARA/>";
	}
	base_status += L"<TEXT>Population: " + Int64ToPrettyStr((INT64)base->HasMarketItem(set_base_crew_type)) + L"</TEXT><PARA/>";

	if (single_vulnerability_window)
	{
		wchar_t buf[75];
		swprintf(buf, _countof(buf), L"<TEXT>Vulnerability Window: %u:00 - %u:%02u</TEXT><PARA/>", base->vulnerabilityWindow1.start / 60, base->vulnerabilityWindow1.end / 60, base->vulnerabilityWindow1.end % 60);
		base_status += buf;
	}
	else
	{
		wchar_t buf[125];
		swprintf(buf, _countof(buf), L"<TEXT>Vulnerability Windows: %u:00 - %u:%02u, %u:00 - %u:%02u</TEXT><PARA/>",
			base->vulnerabilityWindow1.start / 60, base->vulnerabilityWindow1.end / 60, base->vulnerabilityWindow1.end % 60,
			base->vulnerabilityWindow2.start / 60, base->vulnerabilityWindow2.end / 60, base->vulnerabilityWindow2.end % 60);
		base_status += buf;
	}

	if (set_holiday_mode)
	{
		base_status += L"<TEXT>Crew Status: ALL I WANT FOR CHRISTMAS IS YOU</TEXT><PARA/>";
	}
	else if (base->HasMarketItem(set_base_crew_type) < base->base_level * 200)
	{
		base_status += L"<TEXT>Crew Status: Insufficient crew onboard</TEXT><PARA/>";
	}
	else if (base->isCrewSupplied)
	{
		base_status += L"<TEXT>Crew Status: Working</TEXT><PARA/>";
	}
	else
	{
		time_t currTime = time(0);
		uint nextCheckInSeconds = set_crew_check_frequency - (currTime % set_crew_check_frequency);
		uint nextCheckHour = nextCheckInSeconds / 3600;
		uint nextCheckMinute = (nextCheckInSeconds % 3600) / 60;
		base_status += L"<TEXT>Crew Status: Refusing to work over lack of supplies, next supply check in " + stows(itos(nextCheckHour)) + L"h " + stows(itos(nextCheckMinute)) + L"m</TEXT><PARA/>";
	}

	base_status += L"<PARA/>";
	base_status += L"<TEXT>Modules:</TEXT><PARA/>";

	for (uint i = 1; i < base->modules.size(); i++)
	{
		base_status += L"<TEXT>  " + stows(itos(i)) + L": ";
		Module* module = base->modules[i];
		if (module)
		{
			base_status += module->GetInfo(true);
		}
		else
		{
			base_status += L"Empty - available for new module";
		}
		base_status += L"</TEXT><PARA/>";
	}
	base_status += L"<PARA/>";

	base_status += L"<POP/></RDL>";
	SendSetBaseInfoText2(client, base_status);
}

// Update the base status and send it to all clients in the base
void SendBaseStatus(PlayerBase* base)
{
	struct PlayerData* pd = 0;
	while (pd = Players.traverse_active(pd))
	{
		if (!HkIsInCharSelectMenu(pd->iOnlineID))
		{
			if (clients[pd->iOnlineID].player_base == base->base)
			{
				SendBaseStatus(pd->iOnlineID, base);
			}
		}
	}
}

void ForceLaunch(uint client)
{
	uint ship;
	pub::Player::GetShip(client, ship);
	if (ship)
		return;

	uint system;
	pub::Player::GetSystem(client, system);

	wchar_t buf[200];
	_snwprintf(buf, sizeof(buf), L" ChangeSys %u", system);
	SendCommand(client, buf);
}