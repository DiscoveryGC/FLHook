#include "Main.h"


FabricatorModule::FabricatorModule(PlayerBase *the_base)
	: Module(TYPE_FABRICATOR), base(the_base)
{
}

FabricatorModule::~FabricatorModule()
{
}

wstring FabricatorModule::GetInfo(bool xml)
{
	return L"Equipment Fabrication Bay";
}

void FabricatorModule::LoadState(INI_Reader &ini)
{
	while (ini.read_value())
	{
	}
}

void FabricatorModule::SaveState(FILE *file)
{
	fprintf(file, "[FabricatorModule]\n");
}