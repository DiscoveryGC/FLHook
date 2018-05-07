#ifndef __ZONE_UTILITIES_H__
#define __ZONE_UTILITIES_H__ 1

#include <windows.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <math.h>
#include <float.h>

struct TransformMatrix
{
	float d[4][4];
};

struct LOOTABLE_ZONE
{
	/** The zone nickname */
	string zoneNick;

	/** The id of the system for this lootable zone */
	uint systemID;

	/** The nickname and arch id of the loot dropped by the asteroids */
	string lootNick;
	uint iLootID;

	/** The arch id of the crate the loot is dropped in */
	uint iCrateID;

	/** The minimum number of loot items to drop */
	uint iMinLoot;

	/** The maximum number of loot items to drop */
	uint iMaxLoot;

	/** The drop difficultly */
	uint iLootDifficulty;

	/** The lootable zone ellipsoid size */
	Vector size;

	/** The lootable zone position */
	Vector pos;

	/** The zone transformation matrix */
	TransformMatrix transform;
};

namespace ZoneUtilities
{
	void PrintZones();
	bool InZone(uint system, const Vector &pos, LOOTABLE_ZONE &rlz);
}

#endif