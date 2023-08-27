#include "Main.h"

FactoryModule::FactoryModule(PlayerBase* the_base)
	: Module(0), base(the_base)
{
	active_recipe.nickname = 0;
}

// Find the recipe for this building_type and start construction.
FactoryModule::FactoryModule(PlayerBase* the_base, uint nickname)
	: Module(Module::TYPE_FACTORY), factoryNickname(nickname), base(the_base)
{
	active_recipe.nickname = 0;
	for (wstring& craftType : factoryNicknameToCraftTypeMap[factoryNickname])
	{
		base->availableCraftList.insert(craftType);
	}
}

wstring FactoryModule::GetInfo(bool xml)
{
	wstring info;

	std::wstring Status = L"";
	if (Paused)	Status = L"(Paused) ";
	else Status = L"(Active) ";

	info += recipeMap[factoryNickname].infotext;

	wstring openLine;
	wstring closeLine;
	if (xml)
	{
		openLine = L"</TEXT><PARA/><TEXT>      ";
		closeLine = L"</TEXT>";
	}
	else
	{
		openLine = L" - ";
		closeLine = L"\n";
	}
	info += openLine + L"Pending " + stows(itos(build_queue.size())) + L" items";
	if (active_recipe.nickname)
	{
		info += openLine + L"Crafting " + Status + active_recipe.infotext + L". Waiting for:";

		for (auto& i : active_recipe.consumed_items)
		{
			uint good = i.first;
			uint quantity = i.second;

			const GoodInfo* gi = GoodList::find_by_id(good);
			if (gi)
			{
				info += openLine + L"- " + stows(itos(quantity)) + L"x " + HkGetWStringFromIDS(gi->iIDSName);
				if (quantity > 0 && base->HasMarketItem(good) < active_recipe.cooking_rate)
				{
					info += L" [Out of stock]";
				}
			}
		}
		if (active_recipe.credit_cost)
		{
			info += openLine + L" - Credits x" + stows(itos(active_recipe.credit_cost));
			if (base->money < active_recipe.credit_cost)
			{
				info += L" [Insufficient cash]";
			}
		}
		vector<pair<uint, uint>> neededWorkforce;
		if (!active_recipe.catalyst_items.empty())
		{
			info += openLine + L"Needed catalysts:";
			for (const auto& catalyst : active_recipe.catalyst_items)
			{
				if (humanCargoList.count(catalyst.first))
				{
					neededWorkforce.emplace_back(catalyst);
					continue;
				}
				uint good = catalyst.first;
				uint quantity = catalyst.second;

				const GoodInfo* gi = GoodList::find_by_id(good);
				if (gi)
				{
					info += openLine + L" - " + stows(itos(quantity)) + L"x " + HkGetWStringFromIDS(gi->iIDSName);
					uint presentAmount = base->HasMarketItem(good);
					if (presentAmount < quantity)
					{
						info += L" [Need " + stows(itos(quantity - presentAmount)) + L" more]";
					}
				}
			}
		}
		if (!neededWorkforce.empty())
		{
			info += openLine + L"Needed workforce:";
			for (const auto& worker : neededWorkforce)
			{
				uint good = worker.first;
				uint quantity = worker.second;

				const GoodInfo* gi = GoodList::find_by_id(good);
				if (gi)
				{
					info += openLine + L" - " + stows(itos(quantity)) + L"x " + HkGetWStringFromIDS(gi->iIDSName);
					uint presentAmount = base->HasMarketItem(good);
					if (presentAmount < quantity)
					{
						info += L" [Need " + stows(itos(quantity - presentAmount)) + L" more]";
					}
				}
			}
		}
		info += closeLine; 
	}

	return info;
}

// Every 10 seconds we consume goods for the active recipe at the cooking rate
// and if every consumed item has been used then declare the the cooking complete
// and convert this module into the specified type.	
bool FactoryModule::Timer(uint time)
{

	if ((time % set_tick_time) != 0)
	{
		return false;
	}

	// Get the next item to make from the build queue.
	if (!active_recipe.nickname && !build_queue.empty())
	{
		SetActiveRecipe(build_queue.front());
		build_queue.pop_front();
	}

	// Nothing to do.
	if (!active_recipe.nickname || !base->isCrewSupplied || Paused)
	{
		return false;
	}

	// Consume goods at the cooking rate.
	bool cooked = true;

	for (const auto& catalyst : active_recipe.catalyst_items)
	{
		uint good = catalyst.first;
		uint quantityNeeded = catalyst.second;

		uint presentAmount = base->HasMarketItem(good);
		if ((presentAmount - base->reservedCatalystMap[good]) < quantityNeeded)
		{
			return false;
		}
		base->reservedCatalystMap[good] += quantityNeeded;
	}

	if (active_recipe.credit_cost)
	{
		uint moneyToRemove = min(active_recipe.cooking_rate * 10, active_recipe.credit_cost);
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

	for (auto& i : active_recipe.consumed_items)
	{
		uint good = i.first;
		uint quantity = min(active_recipe.cooking_rate, i.second);
		if (!quantity)
		{
			continue;
		}
		cooked = false;
		auto market_item = base->market_items.find(good);
		if (market_item != base->market_items.end()
			&& market_item->second.quantity >= quantity)
		{
			i.second -= quantity;
			base->RemoveMarketGood(good, quantity);
			return false;
		}
	}

	// Do nothing if cooking is not finished
	if (!cooked)
	{
		return false;
	}

	// Add the newly produced item to the market. If there is insufficient space
	// to add the item, wait until there is space.
	for (auto& item : active_recipe.produced_items)
	{
		if (!base->AddMarketGood(item.first, item.second))
		{
			return false;
		}
		else
		{
			item.second = 0;
		}
	}

	if (active_recipe.loop_production)
	{
		// If recipe is set to automatically loop, refresh the recipe data
		SetActiveRecipe(active_recipe.nickname);
	}
	else if (!build_queue.empty())
	{
		// Load next item in the queue
		SetActiveRecipe(build_queue.front());
		build_queue.pop_front();
	}
	else
	{
		active_recipe.nickname = 0;
	}

	return false;
}

void FactoryModule::LoadState(INI_Reader& ini)
{
	active_recipe.nickname = 0;
	RECIPE foundRecipe;
	while (ini.read_value())
	{
		if (ini.is_value("type"))
		{
			factoryNickname = moduleNumberRecipeMap[ini.get_value_int(0)].nickname;
			for (auto& craftType : factoryNicknameToCraftTypeMap[factoryNickname])
			{
				base->availableCraftList.insert(craftType);
				base->craftTypeTofactoryModuleMap[craftType] = this;
			}
			break;
		}
		else if (ini.is_value("nickname"))
		{
			SetActiveRecipe(ini.get_value_int(0));
			active_recipe.consumed_items.clear();
		}
		else if (ini.is_value("paused"))
		{
			Paused = ini.get_value_bool(0);
		}
		else if (ini.is_value("consumed"))
		{
			active_recipe.consumed_items.emplace_back(make_pair(ini.get_value_int(0), ini.get_value_int(1)));
		}
		else if (ini.is_value("credit_cost"))
		{
			active_recipe.credit_cost = ini.get_value_int(0);
		}
		else if (ini.is_value("build_queue"))
		{
			build_queue.emplace_back(ini.get_value_int(0));
		}
	}
}

void FactoryModule::SaveState(FILE* file)
{
	fprintf(file, "[FactoryModule]\n");
	fprintf(file, "type = %u\n", recipeMap[factoryNickname].shortcut_number);
	fprintf(file, "nickname = %u\n", active_recipe.nickname);
	fprintf(file, "paused = %d\n", Paused);
	if (active_recipe.nickname)
	{
		if (active_recipe.credit_cost)
			fprintf(file, "credit_cost = %u\n", active_recipe.credit_cost);
		for (auto& i : active_recipe.consumed_items)
		{
			fprintf(file, "consumed = %u, %u\n", i.first, i.second);
		}
	}
	for (uint i : build_queue)
	{
		fprintf(file, "build_queue = %u\n", i);
	}
}

void FactoryModule::SetActiveRecipe(uint product)
{
	active_recipe = RECIPE(recipeMap[product]);
	if (active_recipe.affiliationBonus.count(base->affiliation))
	{
		float productionModifier = active_recipe.affiliationBonus.at(base->affiliation);
		for (auto& item : active_recipe.consumed_items)
		{
			item.second = static_cast<uint>(ceil(static_cast<float>(item.second) * productionModifier));
		}
	}
}

void FactoryModule::AddToQueue(uint product)
{
	if (build_queue.empty())
	{
		SetActiveRecipe(product);
	}
	else
	{
		build_queue.emplace_back(product);
	}
}

bool FactoryModule::ClearQueue()
{
	build_queue.clear();
	return true;
}

void FactoryModule::ClearRecipe()
{
	active_recipe.nickname = 0;
}

bool FactoryModule::ToggleQueuePaused(bool NewState)
{
	bool RememberState = Paused;
	Paused = NewState;
	//return true if value changed
	return RememberState != NewState;
}

FactoryModule* FactoryModule::FindModuleByProductInProduction(PlayerBase* pb, uint searchedProduct)
{
	for (auto& module : pb->modules)
	{
		FactoryModule* facModPtr = dynamic_cast<FactoryModule*>(module);
		if (facModPtr && facModPtr->active_recipe.nickname == searchedProduct)
		{
			return facModPtr;
		}
	}
	return nullptr;
}

void FactoryModule::StopAllProduction(PlayerBase* pb)
{
	for (auto& i : pb->modules)
	{
		FactoryModule* facModPtr = dynamic_cast<FactoryModule*>(i);
		if (facModPtr)
		{
			facModPtr->ClearQueue();
			facModPtr->ClearRecipe();
		}
	}
}

bool FactoryModule::IsFactoryModule(Module* module)
{
	return module->type == Module::TYPE_FACTORY;
}

const RECIPE* FactoryModule::GetFactoryProductRecipe(wstring& craftType, wstring& product)
{
	product = ToLower(product);
	int shortcut_number = ToInt(product);
	if (recipeCraftTypeNumberMap[craftType].count(shortcut_number))
	{
		return &recipeCraftTypeNumberMap[craftType][shortcut_number];
	}
	else if (recipeCraftTypeNameMap[craftType].count(product))
	{
		return &recipeCraftTypeNameMap[craftType][product];
	}
	return nullptr;
}