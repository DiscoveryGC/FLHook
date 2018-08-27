#pragma once
#include <windows.h>
#include <cstdio>
#include <string>
#include <ctime>
#include <cmath>
#include <chrono>
#include <list>
#include <set>
#include <map>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <FLHook.h>
#include <plugin.h>
#include <PluginUtilities.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include "minijson_writer.hpp"

namespace pt = boost::posix_time;
using namespace std;
using namespace std::chrono;