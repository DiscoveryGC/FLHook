#include "Main.h"

void Utility::SetRepNeutral(uint clientObj, uint targetObj, uint reqClientId)
{
	int targetRep;
	int droneRep;
	pub::SpaceObj::GetRep(targetObj, targetRep);
	pub::SpaceObj::GetRep(clientObj, droneRep);

	PrintUserCmdText(reqClientId, L"Debug: Target rep: %i - Drone Rep: %i", targetRep, droneRep);

	pub::Reputation::SetAttitude(droneRep, targetRep, 0.0f);

}

void Utility::SetRepHostile(uint clientObj, uint targetObj, uint reqClientId)
{
	int targetRep;
	int droneRep;
	pub::SpaceObj::GetRep(targetObj, targetRep);
	pub::SpaceObj::GetRep(clientObj, droneRep);

	PrintUserCmdText(reqClientId, L"Debug: Target rep: %i - Drone Rep: %i", targetRep, droneRep);

	pub::Reputation::SetAttitude(droneRep, targetRep, -1.0f);
}
