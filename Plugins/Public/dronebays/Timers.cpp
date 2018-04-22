#include "Main.h"

void Timers::processDroneMaxDistance(map<uint, ClientDroneInfo>& clientDrones)
{
	for (auto dt = clientDrones.begin(); dt != clientDrones.end(); ++dt)
	{

		// If the drone is already retreating, ignore this order
		if (dt->second.isDroneReturning)
			continue;

		// This only matters if the user is in space
		uint carrierShip;
		pub::Player::GetShip(dt->first, carrierShip);

		if (!carrierShip)
			continue;

		// This also only matters if there is a drone to move
		if (clientDrones[dt->first].deployedInfo.deployedDroneObj == 0)
			continue;

		// Check to see if the distance is greater than the maximum distance
		static uint droneSpaceObj = dt->second.deployedInfo.deployedDroneObj;
		const float distance = HkDistance3DByShip(droneSpaceObj, carrierShip);

		// If the drone is furthur than it should be, retreat to it's owner
		if (distance > dt->second.droneBay.droneRange)
		{
			// If the drone is attacking someone, set the reputation back to neutral before returning
			if (dt->second.deployedInfo.lastShipObjTarget != 0)
			{
				Utility::SetRepNeutral(droneSpaceObj, dt->second.deployedInfo.lastShipObjTarget);
			}

			pub::AI::DirectiveGotoOp gotoOp;
			gotoOp.iGotoType = 0;
			gotoOp.iTargetID = carrierShip;
			gotoOp.fRange = 300.0;
			gotoOp.fThrust = 200;
			gotoOp.goto_cruise = true;

			pub::AI::SubmitDirective(droneSpaceObj, &gotoOp);

			dt->second.isDroneReturning = true;
			PrintUserCmdText(dt->first, L"Maximum distance of %u reached from the drone. Disengaging and returning to carrier.", dt->second.droneBay.droneRange);
		}

		else
		{
			dt->second.isDroneReturning = false;
		}
	}
}

void Timers::processDroneDockRequests(map<uint, DroneDespawnWrapper>& despawnList)
{
	// Check the despawn requests for any ships to be removed
	for (auto dt = despawnList.begin(); dt != despawnList.end(); ++dt)
	{
		// Get the distance between the two objects and check that it's smaller than the carriers radius
		float carrierRadius;
		Vector radiusVector{};
		pub::SpaceObj::GetRadius(dt->second.parentObj, carrierRadius, radiusVector);
		const float shipDistance = abs(HkDistance3DByShip(dt->second.parentObj, dt->second.droneObj));

		// We give a padding of 100 meters to ensure that there is no collision
		if (shipDistance < (carrierRadius + 100))
		{
			pub::SpaceObj::Destroy(dt->second.droneObj, DestroyType::VANISH);
			PrintUserCmdText(dt->first, L"Drone docked");
			despawnList.erase(dt->first);
			clientDroneInfo.erase(dt->first);
		}
	}
}

void Timers::processDroneBuildRequests(map<uint, DroneBuildTimerWrapper>& buildList)
{
	const mstime now = timeInMS();
	uint curr_time = static_cast<uint>(time(nullptr));

	// Check the launch timers for each client
	for (auto dt = buildTimerMap.begin(); dt != buildTimerMap.end(); ++dt)
	{
		// Verify that the user isn't in a tradelane or cruising
		const ENGINE_STATE engineState = HkGetEngineState(dt->first);
		if(engineState == ES_TRADELANE || engineState == ES_CRUISE)
		{
			PrintUserCmdText(dt->first, L"Engine state inoppertune for drone deployment, aborting launch");
			clientDroneInfo[dt->first].buildState = STATE_DRONE_OFF;
			buildTimerMap.erase(dt);
		}

		if ((dt->second.startBuildTime + (dt->second.buildTimeRequired * 1000)) < now)
		{
			Utility::DeployDrone(dt->first, dt->second);
			clientDroneInfo[dt->first].buildState = STATE_DRONE_LAUNCHED;
			buildTimerMap.erase(dt);
		}
	}
}

