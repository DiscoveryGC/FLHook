#include "Main.h"

bool UserCommands::UserCmd_Deploy(uint iClientID, const wstring& wscCmd, const wstring& wscParam, const wchar_t* usage)
{

	//Verify that the user is in space
	uint playerShip;
	pub::Player::GetShip(iClientID, playerShip);
	if(!playerShip)
	{
		PrintUserCmdText(iClientID, L"ERR Not in space");
		return true;
	}

	// Check that the user has a valid bay mounted, if so, get the struct associated with it
	BayArch bayArch;
	bool foundBay = false;
	for (auto& item : Players[iClientID].equipDescList.equip)
	{
		// Early exit from the loop if a bay has been found in the nested loop
		if (foundBay)
			break;

		if (item.bMounted) 
		{
			if(availableDroneBays.find(item.iArchID) != availableDroneBays.end())
			{
				foundBay = true;
				bayArch = availableDroneBays[item.iArchID];
				break;
			}
		}
	}

	if (!foundBay)
	{
		PrintUserCmdText(iClientID, L"No valid mounted drone bay found");
		return true;
	}

	clientDroneInfo[iClientID].droneBay = bayArch;

	// Verify that the user doesn't already have a drone in space
	if(clientDroneInfo[iClientID].deployedInfo.deployedDroneObj != 0)
	{
		PrintUserCmdText(iClientID, L"You may only have one drone deployed at a time");
		return true;
	}

	// Verify that the user isn't already building a drone
	if(clientDroneInfo[iClientID].buildState != STATE_DRONE_OFF)
	{
		PrintUserCmdText(iClientID, L"You are already prepping a drone for takeoff! Use <insert command> to cancel the order");
		return true;
	}

	//Get the drone type argument - We don't care about any garbage after the first space 
	const string reqDroneType = wstos(GetParam(wscParam, L' ', 0));

	// Verify that the requested drone type is a member of the bay's available drones
	if(availableDroneArch.find(reqDroneType) == availableDroneArch.end())
	{
		PrintUserCmdText(iClientID, L"Your drone bay does not support this type of deployment");
		return true;
	}

	const DroneArch requestedDrone = availableDroneArch[reqDroneType];

	// All of the required information is present! Build the timer struct and add it to the list
	DroneTimerWrapper wrapper;
	wrapper.buildTimeRequired = clientDroneInfo[iClientID].droneBay.iDroneBuildTime;
	wrapper.reqDrone = requestedDrone;
	wrapper.startBuildTime = timeInMS();

	buildTimerMap[iClientID] = wrapper;

	// Set the buildstate, and alert the user
	clientDroneInfo[iClientID].buildState = STATE_DRONE_BUILDING;
	PrintUserCmdText(iClientID, L"Drone being prepared for deployment :: Launch in ETA %i seconds", clientDroneInfo[iClientID].droneBay.iDroneBuildTime);
	return true;
}

bool UserCommands::UserCmd_AttackTarget(uint iClientID, const wstring& wscCmd, const wstring& wscParam,
	const wchar_t* usage)
{
	// Verify that the user is in space
	uint iShipObj;
	pub::Player::GetShip(iClientID, iShipObj);
	if(!iShipObj)
	{
		PrintUserCmdText(iClientID, L"You must be in space to use this command");
		return true;
	}

	// Verify that the user has a drone currently deployed
	if(clientDroneInfo[iClientID].deployedInfo.deployedDroneObj == 0)
	{
		PrintUserCmdText(iClientID, L"You must have a drone deployed for this to work");
		return true;
	}

	// Get the players current target
	uint iTargetObj;
	pub::SpaceObj::GetTarget(iShipObj, iTargetObj);

	if(!iTargetObj)
	{
		PrintUserCmdText(iClientID, L"Please target the vessel which the drones should be directed to");
		return true;
	}

	const uint droneObj = clientDroneInfo[iClientID].deployedInfo.deployedDroneObj;

	// Set the old target to neutral reputation, and the hostile one hostile.
	Utility::SetRepNeutral(droneObj, clientDroneInfo[iClientID].deployedInfo.lastShipObjTarget, iClientID);
	Utility::SetRepHostile(droneObj, iTargetObj, iClientID);
	clientDroneInfo[iClientID].deployedInfo.lastShipObjTarget = iTargetObj;
	return true;
}

bool UserCommands::UserCmd_EnterFormation(uint iClientID, const wstring& wscCmd, const wstring& wscParam,
	const wchar_t* usage)
{

}

bool UserCommands::UserCmd_RecallDrone(uint iClientID, const wstring& wscCmd, const wstring& wscParam,
	const wchar_t* usage)
{

}

bool UserCommands::UserCmd_Debug(uint iClientID, const wstring& wscCmd, const wstring& wscParam, const wchar_t* usage)
{
	// For debugging, list the contents of the users dronemap
	ClientDroneInfo info = clientDroneInfo[iClientID];
	PrintUserCmdText(iClientID, L"Current drone ID: %u", info.deployedInfo.deployedDroneObj);
	PrintUserCmdText(iClientID, L"Current state: %u", info.buildState);
	return true;
}

