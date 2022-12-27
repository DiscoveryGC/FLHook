#include "Main.h"

FactoryModule::FactoryModule(PlayerBase *the_base)
	: Module(0), base(the_base)
{
	active_recipe.nickname = 0;
}

// Find the recipe for this building_type and start construction.
FactoryModule::FactoryModule(PlayerBase *the_base, uint the_type)
	: Module(the_type), base(the_base)
{
	active_recipe.nickname = 0;
}

wstring FactoryModule::GetInfo(bool xml)
{
	wstring info;

	std::wstring Status = L"";
	if (Paused)	Status = L"(Paused) ";
	else Status = L"(Active) ";

	info += recipeNumberModuleMap[type].infotext;

	if (xml)
	{
		info += L"</TEXT><PARA/><TEXT>      Pending " + stows(itos(build_queue.size())) + L" items</TEXT>";
		if (active_recipe.nickname)
		{
			info += L"<PARA/><TEXT>      Building " + Status + active_recipe.infotext + L". Waiting for:</TEXT>";

			for (map<uint, uint>::iterator i = active_recipe.consumed_items.begin();
				i != active_recipe.consumed_items.end(); ++i)
			{
				uint good = i->first;
				uint quantity = i->second;

				const GoodInfo *gi = GoodList::find_by_id(good);
				if (gi)
				{
					info += L"<PARA/><TEXT>      - " + stows(itos(quantity)) + L"x " + HkGetWStringFromIDS(gi->iIDSName);
					if (quantity > 0 && base->HasMarketItem(good) < active_recipe.cooking_rate)
						info += L" [Out of stock]";
					info += L"</TEXT>";
				}
			}
		}
		info += L"<TEXT>";
	}
	else
	{
		if (active_recipe.nickname)
		{
			info += L" - Building " + Status + active_recipe.infotext + L". Waiting for:";

			for (map<uint, uint>::iterator i = active_recipe.consumed_items.begin();
				i != active_recipe.consumed_items.end(); ++i)
			{
				uint good = i->first;
				uint quantity = i->second;

				const GoodInfo *gi = GoodList::find_by_id(good);
				if (gi)
				{
					info += L" " + stows(itos(quantity)) + L"x " + HkGetWStringFromIDS(gi->iIDSName);
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

	if ((time%set_tick_time) != 0)
		return false;

	// Get the next item to make from the build queue.
	if (!active_recipe.nickname && build_queue.size())
	{
		map<uint, RECIPE>::iterator i = recipeMap.find(build_queue.front());
		if (i != recipeMap.end())
		{
			active_recipe = i->second;
		}
		build_queue.pop_front();
	}

	// Nothing to do.
	if (!active_recipe.nickname)
		return false;

	if (Paused)
		return false;

	// Consume goods at the cooking rate.
	bool cooked = true;
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

	// Do nothing if cooking is not finished
	if (!cooked)
		return false;

	// Add the newly produced item to the market. If there is insufficient space
	// to add the item, wait until there is space.
	if (!base->AddMarketGood(active_recipe.produced_item, active_recipe.produced_amount))
		return false;


	// If recipe is set to automatically loop, add it back into the queue upon success
	// and prevent wiping the acive_recipe
	if (active_recipe.loop_production && build_queue.empty()) {
		build_queue.push_back(active_recipe.nickname);
	}

	// Reset the nickname to load a new item from the build queue
	// next time around.
	active_recipe.nickname = 0;
	return false;
}

void FactoryModule::LoadState(INI_Reader &ini)
{
	active_recipe.nickname = 0;
	RECIPE foundRecipe;
	while (ini.read_value())
	{
		if (ini.is_value("type"))
		{
			type = ini.get_value_int(0);
		}
		else if (ini.is_value("nickname"))
		{
			active_recipe.nickname = ini.get_value_int(0);
			foundRecipe = recipeMap[active_recipe.nickname];
			active_recipe.produced_item = foundRecipe.produced_item;
			active_recipe.produced_amount = foundRecipe.produced_amount;
			active_recipe.loop_production = foundRecipe.loop_production;
			active_recipe.cooking_rate = foundRecipe.cooking_rate;
			active_recipe.infotext = foundRecipe.infotext;
		}
		else if (ini.is_value("paused"))
		{
			Paused = ini.get_value_bool(0);
		}
		else if (ini.is_value("consumed"))
		{
			active_recipe.consumed_items[ini.get_value_int(0)] = ini.get_value_int(1);
		}
		else if (ini.is_value("build_queue"))
		{
			build_queue.push_back(ini.get_value_int(0));
		}
	}
}

void FactoryModule::SaveState(FILE *file)
{
	fprintf(file, "[FactoryModule]\n");
	fprintf(file, "type = %u\n", type);
	fprintf(file, "nickname = %u\n", active_recipe.nickname);
	fprintf(file, "paused = %d\n", Paused);
	for (map<uint, uint>::iterator i = active_recipe.consumed_items.begin();
		i != active_recipe.consumed_items.end(); ++i)
	{
		fprintf(file, "consumed = %u, %u\n", i->first, i->second);
	}
	for (list<uint>::iterator i = build_queue.begin();
		i != build_queue.end(); ++i)
	{
		fprintf(file, "build_queue = %u\n", *i);
	}
}

bool FactoryModule::AddToQueue(uint product, wstring product_type, wstring factory_type)
{
	//check if product can be produced at the target factory
	if (product_type == factory_type)
	{
		build_queue.push_back(product);
		return true;
	}
	return false;
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
	return RememberState;
}

FactoryModule* FactoryModule::FindModuleByProductInProduction(PlayerBase* pb, uint searchedProduct) {
	FactoryModule* facModPtr = 0;
	for (std::vector<Module*>::iterator i = pb->modules.begin(); i < pb->modules.end(); ++i) {
		facModPtr = dynamic_cast<FactoryModule*>(*i);
		if(facModPtr && facModPtr->active_recipe.nickname == searchedProduct){
			return facModPtr;
		}
	}
	return 0;
}

FactoryModule* FactoryModule::FindFirstFreeModuleByType(PlayerBase* pb, uint searchedType){
	FactoryModule* facModPtr = 0;
	for (std::vector<Module*>::iterator i = pb->modules.begin(); i < pb->modules.end(); ++i) {
		facModPtr = dynamic_cast<FactoryModule*>(*i);
		if (facModPtr && facModPtr->type == searchedType && facModPtr->build_queue.empty()) {
			return facModPtr;
		}
	}
	return 0;
}

void FactoryModule::StopAllModulesOfType(PlayerBase* pb, uint searchedType) {
	FactoryModule* facModPtr = 0;
	for (std::vector<Module*>::iterator i = pb->modules.begin(); i < pb->modules.end(); ++i) {
		facModPtr = dynamic_cast<FactoryModule*>(*i);
		if (facModPtr && facModPtr->type == searchedType){
			facModPtr->ClearQueue();
			facModPtr->ClearRecipe();
		}
	}
}

bool FactoryModule::IsFactoryModule(Module* module) {
	return (module &&
		(module->type == Module::TYPE_M_CLOAK
			|| module->type == Module::TYPE_M_HYPERSPACE_SCANNER
			|| module->type == Module::TYPE_M_JUMPDRIVES
			|| module->type == Module::TYPE_M_DOCKING
			|| module->type == Module::TYPE_M_CLOAKDISRUPTOR));
}

uint FactoryModule::GetRefineryProduct(wstring product) {
	transform(product.begin(), product.end(), product.begin(), ::tolower);
	int shortcut_number = ToInt(product);
	if (recipeNumberRefineryMap.count(shortcut_number)) {
		return recipeNumberRefineryMap[shortcut_number].nickname;
	}
	else if (recipeNameMap.count(product)) {
		return recipeNameMap[product].nickname;
	}
	else {
		return 0;
	}
}

uint FactoryModule::GetFactoryProduct(wstring product) {
	transform(product.begin(), product.end(), product.begin(), ::tolower);
	int shortcut_number = ToInt(product);
	if (recipeNumberFactoryMap.count(shortcut_number)) {
		return recipeNumberFactoryMap[shortcut_number].nickname;
	}
	else if (recipeNameMap.count(product)){
		return recipeNameMap[product].nickname;
	}
	return 0;
}