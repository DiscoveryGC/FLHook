#include "Main.h"

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

			// Log event
			const wstring charname = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(dt->first));
			wstring logString = L"Player %s has docked its drone";
			logString = ReplaceStr(logString, L"%s", charname);
			Utility::LogEvent(wstos(logString).c_str());
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
		if (engineState == ES_TRADELANE || engineState == ES_CRUISE)
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

			// Log event
			const wstring charname = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(dt->first));
			wstring logString = L"Player %player has launched a drone in system %system";
			logString = ReplaceStr(logString, L"%player", charname);
			logString = ReplaceStr(logString, L"%system", HkGetPlayerSystem(dt->first));
			Utility::LogEvent(wstos(logString).c_str());
		}
	}
}

