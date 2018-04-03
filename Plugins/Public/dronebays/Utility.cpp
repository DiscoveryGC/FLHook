#include "Main.h"

void Utility::SetRepNeutral(uint clientObj, uint targetObj)
{
	int targetRep;
	int droneRep;
	pub::SpaceObj::GetRep(targetObj, targetRep);
	pub::SpaceObj::GetRep(clientObj, droneRep);

	pub::Reputation::SetAttitude(droneRep, targetRep, 0.0f);

}

void Utility::SetRepHostile(uint clientObj, uint targetObj)
{
	int targetRep;
	int droneRep;
	pub::SpaceObj::GetRep(targetObj, targetRep);
	pub::SpaceObj::GetRep(clientObj, droneRep);

	pub::Reputation::SetAttitude(droneRep, targetRep, -1.0f);
}

void Utility::DeployDrone(uint iClientID, const DroneBuildTimerWrapper& timerWrapper)
{
	// Set the users client state to reflect a drone has been deployed
	clientDroneInfo[iClientID].buildState = STATE_DRONE_LAUNCHED;

	// Get the current system and location of the carrier
	uint iShip;
	Vector shipPos{};
	Matrix shipRot{};
	uint shipSys;

	pub::Player::GetShip(iClientID, iShip);
	pub::SpaceObj::GetLocation(iShip, shipPos, shipRot);
	pub::SpaceObj::GetSystem(iShip, shipSys);

	// If the ship doesn't exist, the carrier has docked. This should already be taken care of by the docking hook, but abort anyways.
	if (!iShip)
	{
		PrintUserCmdText(iClientID, L"Info [DroneBays] :: You've docked and somehow managed to skip a Hook. Contact a dev-team FLHooker about this bug please!");
		return;
	}

	Utility::CreateNPC(iClientID, shipPos, shipRot, shipSys, timerWrapper.reqDrone);
}

float Utility::RandFloatRange(float a, float b)
{
	return ((b - a)*(static_cast<float>(rand()) / RAND_MAX)) + a;
}

static uint incrementDroneVal = 1;

void Utility::CreateNPC(uint iClientID, Vector pos, Matrix rot, uint iSystem, DroneArch drone)
{

	pub::SpaceObj::ShipInfo si;
	memset(&si, 0, sizeof(si));
	si.iFlag = 1;
	si.iSystem = iSystem;
	si.iShipArchetype = drone.archetype;
	si.vPos = pos;
	si.vPos.x = pos.x + RandFloatRange(0, 500);
	si.vPos.y = pos.y + RandFloatRange(0, 500);
	si.vPos.z = pos.z + RandFloatRange(0, 1000);
	si.mOrientation = rot;
	si.iLoadout = drone.loadout;
	si.iLook1 = CreateID("li_newscaster_head_gen_hat");
	si.iLook2 = CreateID("pl_female1_journeyman_body");
	si.iComm = CreateID("comm_br_darcy_female");
	si.iPilotVoice = CreateID("pilot_f_leg_f01a");
	si.iHealth = -1;
	si.iLevel = 19;

	// Define the string used for the scanner name. Because the
	// following entry is empty, the pilot_name is used. This
	// can be overriden to display the ship type instead.
	FmtStr scanner_name(0, 0);
	scanner_name.begin_mad_lib(0);
	scanner_name.end_mad_lib();

	// Define the string used for the pilot name. The example
	// below shows the use of multiple part names.
	FmtStr pilot_name(0, 0);
	pilot_name.begin_mad_lib(16163); // ids of "%s0 %s1"
	pilot_name.append_string(rand_name());  // ids that replaces %s0
	pilot_name.append_string(rand_name()); // ids that replaces %s1
	pilot_name.end_mad_lib();

	uint rep;
	pub::Reputation::GetReputationGroup(rep, (string("fc_random") + itos(incrementDroneVal)).c_str());

	pub::Reputation::Alloc(si.iRep, scanner_name, pilot_name);
	pub::Reputation::SetAffiliation(si.iRep, rep);

	uint iSpaceObj;
	pub::SpaceObj::Create(iSpaceObj, si);

	pub::AI::SetPersonalityParams pers = Utility::MakePersonality();
	pub::AI::SubmitState(iSpaceObj, &pers);

	clientDroneInfo[iClientID].deployedInfo.deployedDroneObj = iSpaceObj;
}
