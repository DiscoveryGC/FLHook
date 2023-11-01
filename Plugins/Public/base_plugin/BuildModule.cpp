#include "Main.h"

BuildModule::BuildModule(PlayerBase* the_base)
	: Module(TYPE_BUILD), base(the_base)
{
}

// Find the recipe for this building_type and start construction.
BuildModule::BuildModule(PlayerBase* the_base, const RECIPE* module_recipe)
	: Module(TYPE_BUILD), base(the_base), active_recipe(RECIPE(*module_recipe))
{
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

		for (auto& i = active_recipe.consumed_items.begin();
			i != active_recipe.consumed_items.end(); ++i)
		{
			uint good = i->first;
			uint quantity = i->second;

			const GoodInfo* gi = GoodList::find_by_id(good);
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
			info += L"<PARA/><TEXT>      - Credits $" + UIntToPrettyStr(active_recipe.credit_cost);
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

		for (auto& i = active_recipe.consumed_items.begin();
			i != active_recipe.consumed_items.end(); ++i)
		{
			uint good = i->first;
			uint quantity = i->second;

			const GoodInfo* gi = GoodList::find_by_id(good);
			if (gi)
			{
				info += stows(itos(quantity)) + L"x" + HkGetWStringFromIDS(gi->iIDSName) + L" ";
				if (base->HasMarketItem(good) < quantity)
					info += L" [Out of stock]";
			}
		}
		if (active_recipe.credit_cost)
		{
			info += L"Credits $" + UIntToPrettyStr(active_recipe.credit_cost);
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

	if ((time % set_tick_time) != 0)
		return false;

	if (Paused || !base->isCrewSupplied)
		return false;

	bool cooked = true;

	if (active_recipe.credit_cost)
	{
		uint moneyToRemove = min(active_recipe.cooking_rate * 100, active_recipe.credit_cost);
		if (base->money >= moneyToRemove)
		{
			base->money -= moneyToRemove;
			active_recipe.credit_cost -= moneyToRemove;
		}
		if (active_recipe.credit_cost)
		{
			cooked = false;
		}
	}


	for (auto& i = active_recipe.consumed_items.begin(); i != active_recipe.consumed_items.end(); i++)
	{
		uint good = i->first;
		uint quantity = min(active_recipe.cooking_rate, i->second);
		auto market_item = base->market_items.find(good);
		if (market_item == base->market_items.end()
			|| market_item->second.quantity < quantity)
		{
			cooked = false;
			continue;
		}
		i->second -= quantity;
		base->RemoveMarketGood(good, quantity);
		if (!i->second)
		{
			active_recipe.consumed_items.erase(i);
			if (!active_recipe.consumed_items.empty())
			{
				cooked = false;
			}
		}
		else
		{
			cooked = false;
		}
		break;
	}

	// Once cooked turn this into the build type
	if (cooked)
	{
		bool builtCore = false;
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
					base->modules[i] = nullptr;
					builtCore = true;

					// Delete and respawn the old core module
					delete base->modules[0];

					base->modules[0] = new CoreModule(base);
					base->modules[0]->Spawn();

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
				case Module::TYPE_FACTORY:
					//check if factory
					if (factoryNicknameToCraftTypeMap.count(active_recipe.nickname))
					{
						base->modules[i] = new FactoryModule(base, active_recipe.nickname);
						break;
					}
					base->modules[i] = nullptr;
					break;
				 default:
					base->modules[i] = nullptr;
				}
				base->Save();
				delete this;
				return false;
			}
		}

		if (builtCore)
		{
			base->modules.resize((base->base_level * 3) + 1);
		}
	}

	return false;
}

void BuildModule::LoadState(INI_Reader& ini)
{
	while (ini.read_value())
	{
		if (ini.is_value("build_type"))
		{
			uint nickname = CreateID(ini.get_value_string());
			if (!recipeMap.count(nickname))
			{
				return;
			}
			active_recipe = recipeMap.at(nickname);
			active_recipe.consumed_items.clear();
			active_recipe.credit_cost = 0;
		}
		else if (ini.is_value("paused"))
		{
			Paused = ini.get_value_bool(0);
		}
		else if (ini.is_value("consumed"))
		{
			uint good = ini.get_value_int(0);
			uint quantity = ini.get_value_int(1);
			if (quantity)
			{
				active_recipe.consumed_items.emplace_back(make_pair(good, quantity));
			}
		}
		else if (ini.is_value("credit_cost"))
		{
			active_recipe.credit_cost = ini.get_value_int(0);
		}
	}
}

void BuildModule::SaveState(FILE* file)
{
	fprintf(file, "[BuildModule]\n");
	fprintf(file, "build_type = %s\n", active_recipe.nicknameString.c_str());
	fprintf(file, "paused = %d\n", Paused);
	for (auto& i = active_recipe.consumed_items.begin();
		i != active_recipe.consumed_items.end(); ++i)
	{
		if (i->second)
		{
			fprintf(file, "consumed = %u, %u\n", i->first, i->second);
		}
	}
	if (active_recipe.credit_cost)
	{
		fprintf(file, "credit_cost = %u", active_recipe.credit_cost);
	}
}

const RECIPE* BuildModule::GetModuleRecipe(wstring& module_name, wstring& build_list)
{
	module_name = ToLower(module_name);
	uint shortcut_number = ToInt(module_name);
	if (craftListNumberModuleMap.count(build_list) && craftListNumberModuleMap[build_list].count(shortcut_number))
	{
		return &craftListNumberModuleMap[build_list][shortcut_number];
	}
	else if (moduleNameRecipeMap.count(module_name))
	{
		return &moduleNameRecipeMap[module_name];
	}
	return 0;
}