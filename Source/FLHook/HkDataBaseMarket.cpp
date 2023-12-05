#include "hook.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool HkLoadBaseMarket()
{
	INI_Reader ini;

	if (!ini.open("..\\data\\equipment\\market_misc.ini", false))
		return false;

	while (ini.read_header())
	{
		if (!ini.is_header("BaseGood"))
			continue;
		if (!ini.read_value())
			continue;
		if (!ini.is_value("base"))
			continue;

		const char *szBaseName = ini.get_value_string();
		const auto& baseIter = lstBases.find(CreateID(szBaseName));
		if (baseIter == lstBases.end())
		{
			continue;
		}

		BASE_INFO* biBase = &baseIter->second;

		ini.read_value();

		biBase->lstMarketMisc.clear();
		if (!ini.is_value("MarketGood"))
			continue;

		do {
			DATA_MARKETITEM mi;
			const char *szEquipName = ini.get_value_string(0);
			mi.iArchID = CreateID(szEquipName);
			mi.fRep = ini.get_value_float(2);
			biBase->lstMarketMisc.push_back(mi);
		} while (ini.read_value());
	}

	ini.close();
	return true;
}