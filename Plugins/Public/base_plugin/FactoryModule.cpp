#include "Main.h"

FactoryModule::FactoryModule(PlayerBase* the_base)
	: Module(Module::TYPE_FACTORY), base(the_base)
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
		base->craftTypeTofactoryModuleMap[craftType] = this;
	}
}

wstring FactoryModule::GetInfo(bool xml)
{
	wstring info;

	std::wstring Status = L"";
	if (Paused)
	{
		Status = L"(Paused) ";
	}
	else
	{
		Status = L"(Active) ";
	}

	info += recipeMap[factoryNickname].infotext;

	wstring openLine;
	if (xml)
	{
		openLine = L"</TEXT><PARA/><TEXT>      ";
	}
	else
	{
		openLine = L"\n - ";
	}
	if (!build_queue.empty())
	{
		info += openLine + L"Pending " + stows(itos(build_queue.size())) + L" items";
	}
	if (active_recipe.nickname)
	{
		if (pendingSpace)
		{
			info += openLine + active_recipe.infotext + L": Waiting for free cargo storage" + openLine + L"or available max stock limit to drop off:";
			for (auto item : active_recipe.produced_items)
			{
				if (!item.second)
				{
					continue;
				}
				uint good = item.first;
				uint quantity = item.second;
				const GoodInfo* gi = GoodList::find_by_id(good);
				info += openLine + L"- " + stows(itos(quantity)) + L"x " + HkGetWStringFromIDS(gi->iIDSName);
			}
			return info;
		}

		info += openLine + L"Crafting " + Status + active_recipe.infotext + L". Waiting for:";

		for (auto& i : active_recipe.consumed_items)
		{
			uint good = i.first;
			uint quantity = i.second;
			if (!quantity)
			{
				continue;
			}

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
			info += openLine + L" - Credits $" + UIntToPrettyStr(active_recipe.credit_cost);
			if (base->money < active_recipe.credit_cost)
			{
				info += L" [Insufficient cash]";
			}
		}
		if (!active_recipe.catalyst_items.empty() && !sufficientCatalysts)
		{
			info += openLine + L"Needed catalysts ";

			for (const auto& catalyst : active_recipe.catalyst_items)
			{
				uint good = catalyst.first;
				uint quantity = catalyst.second;

				const GoodInfo* gi = GoodList::find_by_id(good);
				if (gi)
				{
					info += openLine + L" - " + stows(itos(quantity)) + L"x " + HkGetWStringFromIDS(gi->iIDSName);
				}
			}
		}
		if (!active_recipe.catalyst_workforce.empty() && !sufficientCatalysts)
		{
			info += openLine + L"Needed workforce:";
			for (const auto& worker : active_recipe.catalyst_workforce)
			{
				uint good = worker.first;
				uint quantity = worker.second;

				const GoodInfo* gi = GoodList::find_by_id(good);
				if (gi)
				{
					info += openLine + L" - " + stows(itos(quantity)) + L"x " + HkGetWStringFromIDS(gi->iIDSName);
				}
			}
		}
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
	pendingSpace = false;

	for (const auto& catalyst : active_recipe.catalyst_items)
	{
		uint good = catalyst.first;
		int quantityNeeded = catalyst.second;

		int presentAmount = base->HasMarketItem(good);
		if ((presentAmount - base->reservedCatalystMap[good]) < quantityNeeded)
		{
			sufficientCatalysts = false;
			return false;
		}
	}
	for (const auto& workers : active_recipe.catalyst_workforce)
	{
		uint good = workers.first;
		int quantityNeeded = workers.second;

		int presentAmount = base->HasMarketItem(good);
		if ((presentAmount - base->reservedCatalystMap[good]) < quantityNeeded)
		{
			sufficientCatalysts = false;
			return false;
		}
	}

	sufficientCatalysts = true;
	bool consumedAnything = false;
	
	if (active_recipe.credit_cost)
	{
		uint moneyToRemove = min(active_recipe.cooking_rate * 100, active_recipe.credit_cost);
		if (base->money >= moneyToRemove)
		{
			base->money -= moneyToRemove;
			active_recipe.credit_cost -= moneyToRemove;
			consumedAnything = true;
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
		consumedAnything = true;
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

	if (consumedAnything)
	{
		for (const auto& catalyst : active_recipe.catalyst_items)
		{
			base->reservedCatalystMap[catalyst.first] += catalyst.second;
		}
		for (const auto& catalyst : active_recipe.catalyst_workforce)
		{
			base->reservedCatalystMap[catalyst.first] += catalyst.second;
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
			pendingSpace = true;
			return false;
		}
		else
		{
			item.second = 0;
		}
	}

	if (!build_queue.empty())
	{
		// Load next item in the queue
		SetActiveRecipe(build_queue.front());
		build_queue.pop_front();
	}
	else if (active_recipe.loop_production)
	{
		// If recipe is set to automatically loop, refresh the recipe data
		SetActiveRecipe(active_recipe.nickname);
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
			factoryNickname = CreateID(ini.get_value_string(0));
			for (auto& craftType : factoryNicknameToCraftTypeMap[factoryNickname])
			{
				base->availableCraftList.insert(craftType);
				base->craftTypeTofactoryModuleMap[craftType] = this;
			}
		}
		else if (ini.is_value("nickname"))
		{
			uint activeRecipeNickname = ini.get_value_int(0);
			if (activeRecipeNickname)
			{
				SetActiveRecipe(activeRecipeNickname);
				active_recipe.consumed_items.clear();
			}
		}
		else if (ini.is_value("paused"))
		{
			Paused = ini.get_value_bool(0);
		}
		else if (ini.is_value("consumed"))
		{
			uint goodID = ini.get_value_int(0);
			uint amount = ini.get_value_int(1);
			if (active_recipe.nickname && amount)
			{
				active_recipe.consumed_items.emplace_back(make_pair(goodID, amount));
			}
		}
		else if (ini.is_value("credit_cost"))
		{
			if (active_recipe.nickname)
			{
				active_recipe.credit_cost = ini.get_value_int(0);
			}
		}
		else if (ini.is_value("build_queue"))
		{
			if (active_recipe.nickname)
			{
				build_queue.emplace_back(ini.get_value_int(0));
			}
		}
	}
}

void FactoryModule::SaveState(FILE* file)
{
	fprintf(file, "[FactoryModule]\n");
	fprintf(file, "type = %s\n", recipeMap[factoryNickname].nicknameString.c_str());
	if (active_recipe.nickname)
	{
		fprintf(file, "nickname = %u\n", active_recipe.nickname);
		fprintf(file, "paused = %d\n", Paused);
		if (active_recipe.credit_cost)
			fprintf(file, "credit_cost = %u\n", active_recipe.credit_cost);
		for (auto& i : active_recipe.consumed_items)
		{
			if (i.second)
			{
				fprintf(file, "consumed = %u, %u\n", i.first, i.second);
			}
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
		active_recipe.credit_cost = static_cast<uint>(ceil(static_cast<float>(active_recipe.credit_cost) * productionModifier));
		active_recipe.cooking_rate = static_cast<uint>(ceil(static_cast<float>(active_recipe.cooking_rate) * productionModifier));
		for (auto& item : active_recipe.consumed_items)
		{
			item.second = static_cast<uint>(ceil(static_cast<float>(item.second) * productionModifier));
		}
	}
}

bool FactoryModule::AddToQueue(uint product)
{
	if (!active_recipe.nickname)
	{
		SetActiveRecipe(product);
		return true;
	}
	else if(active_recipe.loop_production && active_recipe.nickname == product)
	{
		return false;
	}
	else
	{
		build_queue.emplace_back(product);
		return true;
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

void FactoryModule::ClearAllProductionQueues(PlayerBase* pb)
{
	for (auto& i : pb->modules)
	{
		FactoryModule* facModPtr = dynamic_cast<FactoryModule*>(i);
		if (facModPtr)
		{
			facModPtr->ClearQueue();
		}
	}
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