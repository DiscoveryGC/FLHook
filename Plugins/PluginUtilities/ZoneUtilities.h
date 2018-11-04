#ifndef __ZONE_UTILITIES_H__
#define __ZONE_UTILITIES_H__ 1

#include <windows.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <math.h>
#include <float.h>

namespace ZoneUtilities {

	struct SYSTEMINFO
	{
		/** The system nickname */
		string sysNick;

		/** The system id */
		uint systemId;

		/** The system scale */
		float scale;
	};

	struct TransformMatrix
	{
		float d[4][4];
	};

	struct ZONE
	{
		/** The system nickname */
		string sysNick;

		/** The zone nickname */
		string zoneNick;

		/** The id of the system for this zone */
		uint systemId;

		/** The zone transformation matrix */
		TransformMatrix transform;

		/** The zone ellipsoid size */
		Vector size;

		/** The zone position */
		Vector pos;

		/** The damage this zone causes per second */
		int damage;

		/** Is this an encounter zone */
		bool encounter;

		/** The nickname and arch id of the loot dropped by the asteroids */
		string lootNick;

		/** The hash of the loot */
		uint iLootID;

		/** The arch id of the crate the loot is dropped in */
		uint iCrateID;

		/** The minimum number of loot items to drop */
		uint iMinLoot;

		/** The maximum number of loot items to drop */
		uint iMaxLoot;

		/** The drop difficultly */
		uint iLootDifficulty;
	};

	class JUMPPOINT
	{
	public:
		/** The system nickname */
		string sysNick;

		/** The jump point nickname */
		string jumpNick;

		/** The jump point destination system nickname */
		string jumpDestSysNick;

		/** The id of the system for this jump point. */
		uint System;

		/** The id of the jump point. */
		uint jumpID;

		/** The jump point destination system id */
		uint jumpDestSysID;
	};

	/** A map of system id to system info */
	extern map<uint, SYSTEMINFO> mapSystems;

	/** A map of system id to zones */
	extern multimap<uint, ZONE> zones;
	typedef multimap<uint, ZONE, less<uint> >::value_type zone_map_pair_t;
	typedef multimap<uint, ZONE, less<uint> >::iterator zone_map_iter_t;
	typedef multimap<uint, ZONE, less<uint> > zone_map_t;

	/** A map of system id to jumppoint info */
	extern multimap<uint, JUMPPOINT> jumpPoints;
	typedef multimap<uint, JUMPPOINT, less<uint> >::value_type jumppoint_map_pair_t;
	typedef multimap<uint, JUMPPOINT, less<uint> >::iterator jumppoint_map_iter_t;


	void ReadUniverse();
	bool InZone(uint systemID, const Vector &pos, ZONE &rlz);
	bool InDeathZone(uint systemID, const Vector &pos, ZONE &rlz);
	SYSTEMINFO *GetSystemInfo(uint systemID);
	void PrintZones();

}

#endif