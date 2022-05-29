#include "main.h"

/// Send a command to the client at destination ID 0x9999
void SendCommand(uint client, const wstring &message)
{
	HkFMsg(client, L"<TEXT>" + XMLText(message) + L"</TEXT>");
}

void SendSetBaseInfoText(uint client, const wstring &message)
{
	SendCommand(client, wstring(L" SetBaseInfoText ") + message);
}

void SendSetBaseInfoText2(uint client, const wstring &message)
{
	SendCommand(client, wstring(L" SetBaseInfoText2 ") + message);
}

void SendResetMarketOverride(uint client)
{
	SendCommand(client, L" ResetMarketOverride");
}

// Send a price update to all clients in the player base for a single good
void SendMarketGoodUpdated(PlayerBase *base, uint good, MARKET_ITEM &item)
{
	struct PlayerData *pd = 0;
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
					base->g_MarketViewState = PlayerBase::BASE_SHOWING_BUYFROMPLAYER;
					_snwprintf(buf, sizeof(buf), L" SetMarketOverride %u %u %f %u %u",
						base->proxy_base, good, item.buyprice, 1, 0);
				}
				// If the item is buy only and this is not an admin then it is
				// buy only at the client
				else if (item.min_stock >= item.quantity && !clients[client].admin)
				{
					base->g_MarketViewState = PlayerBase::BASE_SHOWING_BUYFROMPLAYER;
					_snwprintf(buf, sizeof(buf), L" SetMarketOverride %u %u %f %u %u",
						base->proxy_base, good, item.buyprice, 1, 0);
				}
				// Otherwise this item is for sale by the client.
				else
				{
					base->g_MarketViewState = PlayerBase::BASE_SHOWING_SELLTOPLAYER;
					_snwprintf(buf, sizeof(buf), L" SetMarketOverride %u %u %f %u %u",
						base->proxy_base, good, item.sellprice, 0, item.quantity);
				}
				SendCommand(client, buf);
			}
		}
	}
}

// Send a price update to a single client for all goods in the base
void SendMarketGoodSync(PlayerBase *base, uint client)
{
	// Reset the client's market
	SendResetMarketOverride(client);

	// Send a dummy entry if there are no goods at this base
	if (!base->market_items.size())
		SendCommand(client, L" SetMarketOverride 0 0 0 0");

	// Send the market
	base->g_MarketViewState = PlayerBase::BASE_SHOWING_SELLTOPLAYER;
	for (map<uint, MARKET_ITEM>::iterator i = base->market_items.begin();
		i != base->market_items.end(); i++)
	{
		uint good = i->first;
		MARKET_ITEM &item = i->second;
		wchar_t buf[200];
		// If the base has none of the item then it is buy-only at the client.
		if (item.quantity == 0)
		{
			_snwprintf(buf, sizeof(buf), L" SetMarketOverride %u %u %f %u %u",
				base->proxy_base, good, item.buyprice, 1, 0);
		}
		// If the item is buy only and this is not an admin then it is
		// buy only at the client
		else if (item.min_stock >= item.quantity && !clients[client].admin)
		{
			_snwprintf(buf, sizeof(buf), L" SetMarketOverride %u %u %f %u %u",
				base->proxy_base, good, item.buyprice, 1, 0);
		}
		// Otherwise this item is for sale by the client.
		else
		{
			_snwprintf(buf, sizeof(buf), L" SetMarketOverride %u %u %f %u %u",
				base->proxy_base, good, item.sellprice, 0, item.quantity);
		}

		SendCommand(client, buf);

	}
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

void SendBaseStatus(uint client, PlayerBase *base)
{
	const Universe::ISystem *sys = Universe::get_system(base->system);

	wstring base_status = L"<RDL><PUSH/>";
	base_status += L"<TEXT>" + XMLText(base->basename) + L", " + HkGetWStringFromIDS(sys->strid_name) + L"</TEXT><PARA/><PARA/>";

	base_status += base->infocard;

	base_status += L"<TEXT>Base Core: Level " + IntToStr(base->base_level) + L"</TEXT><PARA/>";
	base_status += L"<TEXT>Free Cargo Storage: " + Int64ToPrettyStr(base->GetRemainingCargoSpace()) + L"</TEXT><PARA/>";
	base_status += L"<TEXT>Max Cargo Storage: " + Int64ToPrettyStr(base->GetMaxCargoSpace()) + L"</TEXT><PARA/>";
	base_status += L"<TEXT>Money: " + Int64ToPrettyStr(base->money) + L"</TEXT><PARA/>";
	base_status += L"<TEXT>Hit Points: " + Int64ToPrettyStr((INT64)base->base_health) + L"</TEXT><PARA/>";
	base_status += L"<TEXT>Max Hit Points: " + Int64ToPrettyStr((INT64)base->max_base_health) + L"</TEXT><PARA/>";
	base_status += L"<TEXT>Population: " + Int64ToPrettyStr((INT64)base->HasMarketItem(set_base_crew_type)) + L"</TEXT><PARA/>";

	if (base->affiliation)
	{
		base_status += L"<TEXT>Affiliation: " + HkGetWStringFromIDS(Reputation::get_name(base->affiliation)) + L"</TEXT><PARA/>";
	}
	else
	{
		base_status += L"<TEXT>Affiliation: None</TEXT><PARA/>";
	}

	if (set_holiday_mode)
	{
		base_status += L"<TEXT>Repair Status: ALL I WANT FOR CHRISTMAS IS YOU</TEXT><PARA/>";
	}
	else if (base->repairing)
	{
		base_status += L"<TEXT>Repair Status: Repairing</TEXT><PARA/>";
	}
	else
	{
		base_status += L"<TEXT>Repair Status: Not repairing</TEXT><PARA/>";
	}

	base_status += L"<PARA/>";
	base_status += L"<TEXT>Modules:</TEXT><PARA/>";

	for (uint i = 1; i < base->modules.size(); i++)
	{
		base_status += L"<TEXT>  " + stows(itos(i)) + L": ";
		Module *module = base->modules[i];
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
void SendBaseStatus(PlayerBase *base)
{
	struct PlayerData *pd = 0;
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