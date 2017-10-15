#include <windows.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <math.h>
#include <list>
#include <map>
#include <algorithm>
#include <FLHook.h>
#include <plugin.h>
#include <PluginUtilities.h>
#include <sstream>
#include <fstream>
#include "Main.h"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include "minijson_writer.hpp"

namespace pt = boost::posix_time;

void ExportData::ToHTML()
{
	FILE *file = fopen(set_status_path_html.c_str(), "w");
	if (file)
	{
		fprintf(file, "<html>\n<head><title>Player Base Status</title><style type=text/css>\n");
		fprintf(file, ".ColumnH {FONT-FAMILY: Tahoma; FONT-SIZE: 10pt;  TEXT-ALIGN: left; COLOR: #000000; BACKGROUND: #ECE9D8;}\n");
		fprintf(file, ".Column0 {FONT-FAMILY: Tahoma; FONT-SIZE: 10pt;  TEXT-ALIGN: left; COLOR: #000000; BACKGROUND: #FFFFFF;}\n");
		fprintf(file, "</style></head><body>\n\n");

		fprintf(file, "<table width=\"90%%\" border=\"1\" cellspacing=\"0\" cellpadding=\"2\">\n");

		fprintf(file, "<tr>");
		fprintf(file, "<th class=\"ColumnH\">Base Name</th>");
		fprintf(file, "<th class=\"ColumnH\">Base Affiliation</th>");
		fprintf(file, "<th class=\"ColumnH\">Health (%%)</th>");
		fprintf(file, "<th class=\"ColumnH\">Shield Status</th>");
		fprintf(file, "<th class=\"ColumnH\">Money</th>");
		fprintf(file, "<th class=\"ColumnH\">Description</th>");
		fprintf(file, "<th class=\"ColumnH\">Core Level</th>");
		fprintf(file, "<th class=\"ColumnH\">Defense Mode</th>");
		fprintf(file, "<th class=\"ColumnH\">System</th>");
		fprintf(file, "<th class=\"ColumnH\">Position</th>");
		fprintf(file, "<th class=\"ColumnH\">Whitelisted Tags</th>");
		fprintf(file, "<th class=\"ColumnH\">Blacklisted Tags</th>");
		fprintf(file, "</tr>\n\n");

		map<uint, PlayerBase*>::iterator iter = player_bases.begin();
		while (iter != player_bases.end())
		{
			PlayerBase *base = iter->second;

			//do nothing if it's something we don't care about
			if ( mapArchs[base->basetype].display == false)
			{
				++iter;
			}
			else
			{
				wstring theaffiliation = HtmlEncode(HkGetWStringFromIDS(Reputation::get_name(base->affiliation)));

				fprintf(file, "<tr>");
				fprintf(file, "<td class=\"column0\">%s</td>", wstos(HtmlEncode(base->basename)).c_str());
				fprintf(file, "<td class=\"column0\">%s</td>", wstos(HtmlEncode(theaffiliation)).c_str());
				fprintf(file, "<td class=\"column0\">%0.0f</td>", 100 * (base->base_health/base->max_base_health));
				fprintf(file, "<td class=\"column0\">%s</td>", base->shield_state==PlayerBase::SHIELD_STATE_ACTIVE ? "On" : "Off");
				fprintf(file, "<td class=\"column0\">%I64d</td>", base->money);


				string desc;
				for (int i=1; i<=MAX_PARAGRAPHS; i++)
				{
					desc += "<p>";
					desc += wstos(HtmlEncode(base->infocard_para[i]));
					desc += "</p>";
				}
				fprintf(file, "<td class=\"column0\">%s</td>", desc.c_str());

				// the new fields begin here
				fprintf(file, "<td class=\"column0\">%d</td>", base->base_level);
				fprintf(file, "<td class=\"column0\">%d</td>", base->defense_mode);

				const Universe::ISystem *iSys = Universe::get_system(base->system);
				wstring wscSysName = HkGetWStringFromIDS(iSys->strid_name);
				fprintf(file, "<td class=\"column0\">%s</td>", wstos(wscSysName).c_str());
				fprintf(file, "<td class=\"column0\">%0.0f %0.0f %0.0f</td>", base->position.x, base->position.y, base->position.z);

				string thewhitelist;
				for (list<wstring>::iterator i = base->ally_tags.begin(); i != base->ally_tags.end(); ++i)
				{
					thewhitelist.append(wstos((*i)).c_str());
					thewhitelist.append("\n");
				}

				fprintf(file, "<td class=\"column0\">%s</td>", thewhitelist.c_str());

				string theblacklist;
				for (list<wstring>::iterator i = base->perma_hostile_tags.begin(); i != base->perma_hostile_tags.end(); ++i)
				{
					theblacklist.append(wstos((*i)).c_str());
					theblacklist.append("\n");
				}

				fprintf(file, "<td class=\"column0\">%s</td>", theblacklist.c_str());

				fprintf(file, "</tr>\n");   
				++iter;
			}
		}


		fprintf(file, "</table>\n\n</body><html>\n");
		fclose(file);
	}
}

void ExportData::ToJSON()
{
	stringstream stream;
	minijson::object_writer writer(stream);
	writer.write("timestamp", pt::to_iso_string(pt::second_clock::local_time()));
	minijson::object_writer pwc = writer.nested_object("bases");

	map<uint, PlayerBase*>::iterator iter = player_bases.begin();
	while (iter != player_bases.end())
	{
		PlayerBase *base = iter->second;

		//do nothing if it's something we don't care about
		//if ((base->basetype == "jumpgate") || (base->basetype == "jumphole") || (base->basetype == "airlock") || (base->basetype == "solar") || (base->basetype == "invinciblesolar"))
		//{
		//	++iter;
		//}
		//else
		//{
			//grab the affiliation before we begin
			wstring theaffiliation = HtmlEncode(HkGetWStringFromIDS(Reputation::get_name(base->affiliation)));
			if (theaffiliation == L"Object Unknown")
			{
				theaffiliation = L"No Affiliation";
			}
			
			//begin the object writer
			minijson::object_writer pw = pwc.nested_object( wstos(HtmlEncode(base->basename)).c_str() );
			
			minijson::array_writer pwds = pw.nested_array("passwords");
			// first thing we'll do is grab all administrator passwords, encoded.
			for (list<BasePassword>::iterator it=base->passwords.begin(); it != base->passwords.end(); ++it)
			{
				BasePassword bp = *it;
				wstring l = bp.pass;
				if (!bp.admin && bp.viewshop)
					l += L" viewshop";
				pwds.write(wstos(HtmlEncode(l)).c_str());
			}
			pwds.close();
			
			//add basic elements
			pw.write("affiliation", wstos(HtmlEncode(theaffiliation)).c_str() );
			pw.write("type", base->basetype.c_str());
			pw.write("solar", base->basesolar.c_str());
			pw.write("loadout", base->baseloadout.c_str());
			pw.write("level", base->base_level);
			pw.write("health", 100 * (base->base_health/base->max_base_health));
			pw.write("defensemode", base->defense_mode);
			pw.close();

			++iter;
		//}
		
	}
	pwc.close();

	writer.close();

	//dump to a file
	FILE *file = fopen("c:/stats/base_status.json", "w");
	if (file)
	{
		fprintf(file, stream.str().c_str());
		fclose(file);
	}
}
