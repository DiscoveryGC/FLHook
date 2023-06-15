#include "main.h"

/// Send a command to the client at destination ID 0x9999
void SendCommand(uint client, const wstring &message)
{
	HkFMsg(client, L"<TEXT>" + XMLText(message) + L"</TEXT>");
}

void SendSetBaseInfoText2(UINT client, const wstring &message)
{
	SendCommand(client, wstring(L" SetBaseInfoText2 ") + message);
}

void SendResetMarketOverride(UINT client)
{
	SendCommand(client, L" ResetMarketOverride");
	SendCommand(client, L" SetMarketOverride 0 0 0 0");
}

void ForceLaunch(uint client)
{
	uint ship;
	pub::Player::GetShip(client, ship);
	if (ship)
		return;

	uint systemId;
	pub::Player::GetSystem(client, systemId);

	wchar_t buf[200];
	_snwprintf(buf, sizeof(buf), L" ChangeSys %u", systemId);
	SendCommand(client, buf);
}