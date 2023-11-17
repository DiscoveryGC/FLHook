#include "hook.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

int __stdcall DisconnectPacketSent(uint iClientID)
{
	LOG_CORE_TIMER_START
	TRY_HOOK {
		uint iShip = 0;
		pub::Player::GetShip(iClientID, iShip);
		if (set_iDisconnectDelay && iShip)
		{ // in space
			ClientInfo[iClientID].tmF1TimeDisconnect = timeInMS() + set_iDisconnectDelay;
			CALL_PLUGINS_NORET(PLUGIN_DelayedDisconnect, , (uint, uint), (iClientID, iShip));
			return 0; // don't pass on
		}
	} CATCH_HOOK({})
	LOG_CORE_TIMER_END
	return 1; // pass on
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

FARPROC fpOldDiscPacketSent;

__declspec(naked) void _DisconnectPacketSent()
{
	__asm
	{
		pushad
		mov eax, [esi + 0x68]
		push eax
		call DisconnectPacketSent
		cmp eax, 0
		jz suppress
		popad
		jmp[fpOldDiscPacketSent]
		suppress:
		popad
			mov eax, [hModDaLib]
			add eax, ADDR_DALIB_DISC_SUPPRESS
			jmp eax
	}
}
