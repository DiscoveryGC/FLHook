#include "Main.h"

BuildModule::BuildModule(PlayerBase *the_base)
	: Module(TYPE_BUILD), base(the_base)
{
}

// Find the recipe for this building_type and start construction.
BuildModule::BuildModule(PlayerBase *the_base, RECIPE* module_recipe)
	: Module(TYPE_BUILD), base(the_base)
{
	active_recipe = *module_recipe;
}

wstring BuildModule::GetInfo(bool xml)
{
	wstring info;
	std::wstring Status;
	if (Paused)	Status = L"(Paused) ";
	else Status = L"(Active) ";
	if (xml)
	{

		info = L"<TEXT>Constructing " + Status + active_recipe.infotext + L". Waiting for:</TEXT>";

		for (map<uint, uint>::iterator i = active_recipe.consumed_items.begin();
			i != active_recipe.consumed_items.end(); ++i)
		{
			uint good = i->first;
			uint quantity = i->second;

			const GoodInfo *gi = GoodList::find_by_id(good);
			if (gi)
			{
				info += L"<PARA/><TEXT>      - " + stows(itos(quantity)) + L"x " + HkGetWStringFromIDS(gi->iIDSName);
				if (base->HasMarketItem(good) < quantity)
					info += L" [Out of stock]";
				info += L"</TEXT>";
			}
		}
		if (active_recipe.credit_cost)
		{
			info += L"<PARA/><TEXT>      - Credits x" + stows(itos(active_recipe.credit_cost));
			if (base->money < active_recipe.credit_cost)
			{
				info += L" [Insufficient cash]";
			}
			info += L"</TEXT>";
		}
	}
	else
	{
		info = L"Constructing " + Status + active_recipe.infotext + L". Waiting for: ";

		for (map<uint, uint>::iterator i = active_recipe.consumed_items.begin();
			i != active_recipe.consumed_items.end(); ++i)
		{
			uint good = i->first;
			uint quantity = i->second;

			const GoodInfo *gi = GoodList::find_by_id(good);
			if (gi)
			{
				info += stows(itos(quantity)) + L"x" + HkGetWStringFromIDS(gi->iIDSName) + L" ";
				if (base->HasMarketItem(good) < quantity)
					info += L" [Out of stock]";
			}
		}
		if (active_recipe.credit_cost)
		{
			info += L"Credits x" + stows(itos(active_recipe.credit_cost));
			if (base->money < active_recipe.credit_cost)
			{
				info += L" [Insufficient cash]";
			}
		}
	}

	return info;
}

// Every 10 seconds we consume goods for the active recipe at the cooking rate
// and if every consumed item has been used then declare the the cooking complete
// and convert this module into the specified type.	
bool BuildModule::Timer(uint time)
{

	if ((time%set_tick_time) != 0)
		return false;

	if (Paused || !base->isCrewSupplied)
		return false;

	bool cooked = true;

	if (active_recipe.credit_cost)
	{
		uint moneyToRemove = min(active_recipe.cooking_rate * 10, active_recipe.credit_cost);
		if (base->money >= moneyToRemove)
		{
			base->money -= moneyToRemove;
			active_recipe.credit_cost -= moneyToRemove;
		}
		if (active_recipe.credit_cost) {
			cooked = false;
		}
	}

	// Consume goods at the cooking rate.
	for (map<uint, uint>::iterator i = active_recipe.consumed_items.begin();
		i != active_recipe.consumed_items.end(); ++i)
	{
		uint good = i->first;
		uint quantity = i->second > active_recipe.cooking_rate ? active_recipe.cooking_rate : i->second;
		if (quantity)
		{
			cooked = false;
			map<uint, MARKET_ITEM>::iterator market_item = base->market_items.find(good);
			if (market_item != base->market_items.end())
			{
				if (market_item->second.quantity >= quantity)
				{
					i->second -= quantity;
					base->RemoveMarketGood(good, quantity);
					return false;
				}
			}
		}
	}

	// Once cooked turn this into the build type
	if (cooked)
	{
		for (uint i = 0; i < base->modules.size(); i++)
		{
			if (base->modules[i] == this)
			{
				switch (this->active_recipe.shortcut_number)
				{
				case Module::TYPE_CORE:
					base->base_level++;
					if (base->base_level > 4)
						base->base_level = 4;
					base->SetupDefaults();

					// Clear the build module slot.
					base->modules[i] = 0;

					// Delete and respawn the old core module
					delete base->modules[0];
					base->modules[0] = new CoreModule(base);
					base->modules[0]->Spawn();

					break;
				case Module::TYPE_SHIELDGEN:
					base->modules[i] = new ShieldModule(base);
					break;
				case Module::TYPE_STORAGE:
					base->modules[i] = new StorageModule(base);
					break;
				case Module::TYPE_DEFENSE_1:
					base->modules[i] = new DefenseModule(base, Module::TYPE_DEFENSE_1);
					break;
				case Module::TYPE_DEFENSE_2:
					base->modules[i] = new DefenseModule(base, Module::TYPE_DEFENSE_2);
					break;
				case Module::TYPE_DEFENSE_3:
					base->modules[i] = new DefenseModule(base, Module::TYPE_DEFENSE_3);
					break;
				default:
					//check if factory
					if (factoryNicknameToCraftTypeMap.count(active_recipe.nickname)) {
						base->modules[i] = new FactoryModule(base, active_recipe.nickname);
						break;
					}
					base->modules[i] = 0;
					break;
				}
				base->Save();
				delete this;
				return false;
			}
		}
	}

	return false;
}

void BuildModule::LoadState(INI_Reader &ini)
{
	while (ini.read_value())
	{
		if (ini.is_value("build_type"))
		{
			RECIPE recipe = moduleNumberRecipeMap[ini.get_value_int(0)];
			recipe.consumed_items.clear();
			active_recipe = recipe;
		}
		else if (ini.is_value("consumed"))
		{
			active_recipe.consumed_items[ini.get_value_int(0)] = ini.get_value_int(1);
		}
		else if (ini.is_value("credit_cost"))
		{
			active_recipe.credit_cost = ini.get_value_int(0);
		}
	}
}

void BuildModule::SaveState(FILE *file)
{
	fprintf(file, "[BuildModule]\n");
	fprintf(file, "build_type = %u\n", active_recipe.shortcut_number);
	fprintf(file, "infotext = %s\n", wstos(active_recipe.infotext).c_str());
	for (map<uint, uint>::iterator i = active_recipe.consumed_items.begin();
		i != active_recipe.consumed_items.end(); ++i)
	{
		fprintf(file, "consumed = %u, %u\n", i->first, i->second);
	}
	if (active_recipe.credit_cost)
	{
		fprintf(file, "credit_cost = %u", active_recipe.credit_cost);
	}
}

RECIPE* BuildModule::GetModuleRecipe(wstring module_name, wstring build_list) {
	transform(module_name.begin(), module_name.end(), module_name.begin(), ::tolower);
	uint shortcut_number = ToInt(module_name);
	if (craftListNumberModuleMap.count(build_list) && craftListNumberModuleMap[build_list].count(shortcut_number)) {
		return &craftListNumberModuleMap[build_list][shortcut_number];
	}
	else if (moduleNameRecipeMap.count(module_name)){
		return &moduleNameRecipeMap[module_name];
	}
	return 0;
}