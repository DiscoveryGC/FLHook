// Better POB logs for POB Plugin
// December 2016 by Alley
//
// 
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Includes
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <math.h>
#include <float.h>
#include <FLHook.h>
#include <plugin.h>
#include <math.h>
#include <list>
#include <set>

#include "PluginUtilities.h"
#include "Main.h"

namespace Log
{
	void Logging(string basename, const char *szString, ...)
	{
		
	}

	void LogBaseAction(string basename, const char *message)
	{
		char szBufString[1024];
		va_list marker;
		va_start(marker, message);
		_vsnprintf(szBufString, sizeof(szBufString) - 1, message, marker);

		char szBuf[64];
		time_t tNow = time(0);
		struct tm *t = localtime(&tNow);
		strftime(szBuf, sizeof(szBuf), "%d/%m/%Y %H:%M:%S", t);

		string BuildFilePath = "./flhook_logs/pob/" + basename + ".log";
		FILE *Logfile = fopen((BuildFilePath.c_str()), "at");

		if (Logfile)
		{
			fprintf(Logfile, "%s %s\n", szBuf, szBufString);
			fflush(Logfile);
			fclose(Logfile);
		}
	}

	void LogGenericAction(string message)
	{

	}
}

