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

	if (xml)
	{
		info += L"</TEXT><PARA/><TEXT>      Pending " + stows(itos(build_queue.size())) + L" items</TEXT>";
		if (active_recipe.nickname)
		{
			info += L"<PARA/><TEXT>      Crafting " + Status + active_recipe.infotext + L". Waiting for:</TEXT>";

			for (auto& i = active_recipe.consumed_items.begin();
				i != active_recipe.consumed_items.end(); ++i)
			{
				uint good = i->first;
				uint quantity = i->second;

				const GoodInfo* gi = GoodList::find_by_id(good);
				if (gi)
				{
					info += L"<PARA/><TEXT>      - " + stows(itos(quantity)) + L"x " + HkGetWStringFromIDS(gi->iIDSName);
					if (quantity > 0 && base->HasMarketItem(good) < active_recipe.cooking_rate)
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
			if (!active_recipe.catalyst_items.empty())
			{
				info += L"<PARA/><TEXT>      Needed catalysts:</TEXT>";
				for (const auto& catalyst : active_recipe.catalyst_items)
				{
					uint good = catalyst.first;
					uint quantity = catalyst.second;

					const GoodInfo* gi = GoodList::find_by_id(good);
					if (gi)
					{
						info += L"<PARA/><TEXT>      - " + stows(itos(quantity)) + L"x " + HkGetWStringFromIDS(gi->iIDSName);
						uint presentAmount = base->HasMarketItem(good);
						if (presentAmount < quantity)
							info += L" [Need " + stows(itos(quantity - presentAmount)) + L" more]";
						info += L"</TEXT>";
					}
				}
			}
		}
	}
	else
	{
		if (active_recipe.nickname)
		{
			info += L" - Building " + Status + active_recipe.infotext + L". Waiting for:";

			for (auto& i = active_recipe.consumed_items.begin();
				i != active_recipe.consumed_items.end(); ++i)
			{
				uint good = i->first;
				uint quantity = i->second;

				const GoodInfo* gi = GoodList::find_by_id(good);
				if (gi)
				{
					info += stows(itos(quantity)) + L"x " + HkGetWStringFromIDS(gi->iIDSName);
					if (base->HasMarketItem(good) < quantity)
						info += L" [Out of stock]";
				}
				if (active_recipe.credit_cost)
				{
					info += L"Credits x" + stows(itos(active_recipe.credit_cost));
					if (base->money < active_recipe.credit_cost)
					{
						info += L" [Insufficient cash]";
					}
				}
				if (!active_recipe.catalyst_items.empty())
				{
					info += L"Needed catalysts:";
					for (const auto& catalyst : active_recipe.catalyst_items)
					{
						uint good = catalyst.first;
						uint quantity = catalyst.second;

						const GoodInfo* gi = GoodList::find_by_id(good);
						if (gi)
						{
							info += L" - " + stows(itos(quantity)) + L"x " + HkGetWStringFromIDS(gi->iIDSName);
							uint presentAmount = base->HasMarketItem(good);
							if (presentAmount < quantity)
								info += L" [Need " + stows(itos(quantity - presentAmount)) + L" more]";
						}
					}
				}
			}
		}
		else {
			info += L" - Pending " + stows(itos(build_queue.size())) + L" items ";
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
		return false;

	// Get the next item to make from the build queue.
	if (!active_recipe.nickname && !build_queue.empty())
	{
		SetActiveRecipe(build_queue.front());
		build_queue.pop_front();
	}

	// Nothing to do.
	if (!active_recipe.nickname || !base->isCrewSupplied || Paused)
		return false;

	// Consume goods at the cooking rate.
	bool cooked = true;

	for (const auto& catalyst : active_recipe.catalyst_items)
	{
		uint good = catalyst.first;
		uint quantity = catalyst.second;

		const GoodInfo* gi = GoodList::find_by_id(good);
		if (gi)
		{
			uint presentAmount = base->HasMarketItem(good);
			if (presentAmount < quantity)
				return false;
		}
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

	for (auto& i = active_recipe.consumed_items.begin();
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

	// Do nothing if cooking is not finished
	if (!cooked)
		return false;

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
		for (auto& i = active_recipe.consumed_items.begin();
			i != active_recipe.consumed_items.end(); ++i)
		{
			fprintf(file, "consumed = %u, %u\n", i->first, i->second);
		}
	}
	for (list<uint>::iterator i = build_queue.begin();
		i != build_queue.end(); ++i)
	{
		fprintf(file, "build_queue = %u\n", *i);
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
	for (std::vector<Module*>::iterator i = pb->modules.begin(); i < pb->modules.end(); ++i)
	{
		FactoryModule* facModPtr = dynamic_cast<FactoryModule*>(*i);
		if (facModPtr && facModPtr->active_recipe.nickname == searchedProduct)
		{
			return facModPtr;
		}
	}
	return nullptr;
}

void FactoryModule::StopAllProduction(PlayerBase* pb)
{
	for (std::vector<Module*>::iterator i = pb->modules.begin(); i < pb->modules.end(); ++i)
	{
		FactoryModule* facModPtr = dynamic_cast<FactoryModule*>(*i);
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