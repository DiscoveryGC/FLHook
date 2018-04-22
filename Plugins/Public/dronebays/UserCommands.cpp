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
		PrintUserCmdText(iClientID, L"You are already prepping a drone for takeoff!");
		return true;
	}

	// Verify that the client isn't cruising or in a tradelane
	const ENGINE_STATE engineState = HkGetEngineState(iClientID);
	if(engineState == ES_TRADELANE || engineState == ES_CRUISE)
	{
		PrintUserCmdText(iClientID, L"Engine state inoppertune for drone deployment, aborting launch");
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
	DroneBuildTimerWrapper wrapper;
	wrapper.buildTimeRequired = clientDroneInfo[iClientID].droneBay.iDroneBuildTime;
	wrapper.reqDrone = requestedDrone;
	wrapper.startBuildTime = timeInMS();

	buildTimerMap[iClientID] = wrapper;

	// Set the buildstate, and alert the user
	clientDroneInfo[iClientID].buildState = STATE_DRONE_BUILDING;
	PrintUserCmdText(iClientID, L"Drone being prepared for deployment :: Launch in ETA %i seconds", clientDroneInfo[iClientID].droneBay.iDroneBuildTime);

	// Save the carrier shipObj to the client struct
	clientDroneInfo[iClientID].carrierShipobj = playerShip;
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
	Utility::SetRepNeutral(droneObj, clientDroneInfo[iClientID].deployedInfo.lastShipObjTarget);
	Utility::SetRepHostile(droneObj, iTargetObj);

	// Set the target hostile to the drone as well only if it isn't another existing drone 
	bool isTargetDrone = false;
	for (const auto& currClient : clientDroneInfo)
	{
		if (iTargetObj == currClient.second.deployedInfo.deployedDroneObj)
		{
			isTargetDrone = true;
			break;
		}
	}

	if (!isTargetDrone)
	{
		Utility::SetRepHostile(iTargetObj, droneObj);
	}

	clientDroneInfo[iClientID].deployedInfo.lastShipObjTarget = iTargetObj;

	PrintUserCmdText(iClientID, L"Drone targeting selected ship\n");

	return true;
}


bool UserCommands::UserCmd_RecallDrone(uint iClientID, const wstring& wscCmd, const wstring& wscParam,
	const wchar_t* usage)
{
	// Verify that the user is in space
	uint iShipObj;
	pub::Player::GetShip(iClientID, iShipObj);
	if (!iShipObj)
	{
		PrintUserCmdText(iClientID, L"You must be in space to use this command");
		return true;
	}

	// Verify that the user has a drone currently deployed
	if (clientDroneInfo[iClientID].deployedInfo.deployedDroneObj == 0)
	{
		PrintUserCmdText(iClientID, L"You must have a drone deployed for this to work");
		return true;
	}

	// Set the NPC to fly to your current position
	pub::AI::DirectiveGotoOp gotoOp;
	
	// Type zero says to fly to a spaceObj
	gotoOp.iGotoType = 0;
	gotoOp.iTargetID = iShipObj;
	gotoOp.fRange = 10.0;
	gotoOp.fThrust = 100;
	gotoOp.goto_cruise = true;

	pub::AI::SubmitDirective(clientDroneInfo[iClientID].deployedInfo.deployedDroneObj, &gotoOp);

	//Create the timer entry to keep watch on this docking operation
	DroneDespawnWrapper wrapper;
	wrapper.droneObj = clientDroneInfo[iClientID].deployedInfo.deployedDroneObj;
	wrapper.parentObj = iShipObj;
	
	droneDespawnMap[iClientID] = wrapper;

	PrintUserCmdText(iClientID, L"Drone recall operation sent");

	return true;
}

bool UserCommands::UserCmd_DroneStop(uint iClientID, const wstring& wscCmd, const wstring& wscParam,
	const wchar_t* usage)
{
	// Verify that the user is in space
	uint iShipObj;
	pub::Player::GetShip(iClientID, iShipObj);
	if (!iShipObj)
	{
		PrintUserCmdText(iClientID, L"You must be in space to use this command");
		return true;
	}

	// Verify that the user has a drone currently deployed
	if (clientDroneInfo[iClientID].deployedInfo.deployedDroneObj == 0)
	{
		PrintUserCmdText(iClientID, L"You must have a drone deployed for this to work");
		return true;
	}

	const uint droneObj = clientDroneInfo[iClientID].deployedInfo.deployedDroneObj;

	// Set the drone reputation to neutral with who it was last attacking
	Utility::SetRepNeutral(droneObj, clientDroneInfo[iClientID].deployedInfo.lastShipObjTarget);

	// Send a stop directive to the drone
	pub::AI::DirectiveCancelOp cancelOp;
	SubmitDirective(droneObj, &cancelOp);

	PrintUserCmdText(iClientID, L"Drone operations aborted");
	return true;
}

bool UserCommands::UserCmd_DroneCome(uint iClientID, const wstring& wscCmd, const wstring& wscParam,
	const wchar_t* usage)
{
	// Verify that the user is in space
	uint iShipObj;
	pub::Player::GetShip(iClientID, iShipObj);
	if (!iShipObj)
	{
		PrintUserCmdText(iClientID, L"You must be in space to use this command");
		return true;
	}

	// Verify that the user has a drone currently deployed
	if (clientDroneInfo[iClientID].deployedInfo.deployedDroneObj == 0)
	{
		PrintUserCmdText(iClientID, L"You must have a drone deployed for this to work");
		return true;
	}

	const uint droneObj = clientDroneInfo[iClientID].deployedInfo.deployedDroneObj;

	// Set the drone reputation to neutral with who it was last attacking
	Utility::SetRepNeutral(droneObj, clientDroneInfo[iClientID].deployedInfo.lastShipObjTarget);

	// Set the NPC to fly to your current position
	pub::AI::DirectiveGotoOp gotoOp;

	// Type zero says to fly to a spaceObj
	gotoOp.iGotoType = 0;
	gotoOp.iTargetID = iShipObj;
	gotoOp.fRange = 10.0;
	gotoOp.fThrust = 100;
	gotoOp.goto_cruise = true;

	pub::AI::SubmitDirective(clientDroneInfo[iClientID].deployedInfo.deployedDroneObj, &gotoOp);

	PrintUserCmdText(iClientID, L"Drone disengaging and returning to your position");
	return true;
}

bool UserCommands::UserCmd_DroneBayAvailability(uint iClientID, const wstring& wscCmd, const wstring& wscParam,
	const wchar_t* usage)
{
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
			if (availableDroneBays.find(item.iArchID) != availableDroneBays.end())
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

	// Print out each available drone type for this bay
	PrintUserCmdText(iClientID, L"---Valid drones---");
	for (const auto bay : bayArch.availableDrones)
	{
		PrintUserCmdText(iClientID, stows(bay));
	}
	PrintUserCmdText(iClientID, L"------------------");

	return true;
}



bool UserCommands::UserCmd_Debug(uint iClientID, const wstring& wscCmd, const wstring& wscParam, const wchar_t* usage)
{
	// For debugging, list the contents of the users dronemap
	ClientDroneInfo info = clientDroneInfo[iClientID];
	PrintUserCmdText(iClientID, L"Current drone ID: %u", info.deployedInfo.deployedDroneObj);
	PrintUserCmdText(iClientID, L"Current state: %u", info.buildState);
	return true;
}

bool UserCommands::UserCmd_DroneHelp(uint iClientID, const wstring& wscCmd, const wstring& wscParam,
	const wchar_t* usage)
{
	PrintUserCmdText(iClientID, L"Drone Usage");
	PrintUserCmdText(iClientID, L"/dronetypes - Lists all available dronetypes for your bay type");
	PrintUserCmdText(iClientID, L"/dronedeploy [dronetype] - Launches a drone compatible with your dronebay");
	PrintUserCmdText(iClientID, L"/dronetarget - Directs your drone to attack whatever you are targeting");
	PrintUserCmdText(iClientID, L"/dronestop - Stops your drone from attacking");
	PrintUserCmdText(iClientID, L"/dronerecall - Recalls your drone, and docks it with your carrier");
	PrintUserCmdText(iClientID, L"/dronecome - Disengages the drone from it's current target, and directs it to fly to your position.");

	return true;
}

